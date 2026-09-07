#pragma once

#include <any>
#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <glaze/glaze.hpp>
#include <memory>
#include <thread>
#include <utility>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

#include "DataTypes.hpp"
#include "Env.hpp"
#include "Logging.hpp"

namespace draconis::utils::cache {
  namespace types = ::draconis::utils::types;

  using std::chrono::days;
  using std::chrono::duration_cast;
  using std::chrono::seconds;
  using std::chrono::system_clock;

  namespace fs = std::filesystem;

  enum class CacheLocation : types::u8 {
    InMemory,      ///< Volatile, lost on app exit. Fastest.
    TempDirectory, ///< Persists until next reboot or system cleanup.
    Persistent     ///< Stored in a user-level cache dir (e.g., ~/.cache).
  };

  struct CachePolicy {
    CacheLocation location = CacheLocation::Persistent;

    types::Option<seconds> ttl = days(1); ///< Default to 1 day.

    static auto inMemory() -> CachePolicy {
      return { .location = CacheLocation::InMemory, .ttl = types::None };
    }

    static auto neverExpire() -> CachePolicy {
      return { .location = CacheLocation::Persistent, .ttl = types::None };
    }

    static auto tempDirectory() -> CachePolicy {
      return { .location = CacheLocation::TempDirectory, .ttl = seconds(60) };
    }
  };

  class CacheManager {
   public:
    /*!
     * @brief Global flag to bypass all caching logic at runtime.
     *
     * When set to true, getOrSet() will skip both reading from and writing to
     * any cache (in-memory or on-disk) and will directly invoke the provided
     * fetcher each time. This makes it easy for the CLI to offer a
     * "--ignore-cache" option without having to modify every call-site.
     */
    static inline std::atomic_bool ignoreCache = false;

    // Type-erased cache entries may contain destructors instantiated in a
    // plugin module. Retain that code until every entry has been destroyed.
    auto retainModule(const std::shared_ptr<void>& module) -> void {
      if (!module)
        return;
      const std::lock_guard lock(m_cacheMutex);
      if (std::ranges::find(m_moduleLeases, module) == m_moduleLeases.end())
        m_moduleLeases.push_back(module);
    }

    /**
     * @brief Get the persistent cache directory path for the current platform.
     * @return The path to the cache directory (e.g., ~/.cache/draconis++ on Linux,
     *         ~/Library/Caches/draconis++ on macOS, %LOCALAPPDATA%/draconis++/cache on Windows)
     */
    static auto getPersistentCacheDir() -> fs::path {
#ifdef __APPLE__
      return fs::path(std::format("{}/Library/Caches/draconis++", draconis::utils::env::GetEnv("HOME").value_or(".")));
#elif defined(_WIN32)
      // On Windows, use LOCALAPPDATA for cache (standard Windows location)
      if (auto localAppData = draconis::utils::env::GetEnv("LOCALAPPDATA"))
        return fs::path(*localAppData) / "draconis++" / "cache";
      // Fallback to USERPROFILE if LOCALAPPDATA is not set
      return fs::path(draconis::utils::env::GetEnv("USERPROFILE").value_or(".")) / ".cache" / "draconis++";
#else
      return fs::path(std::format("{}/.cache/draconis++", draconis::utils::env::GetEnv("HOME").value_or(".")));
#endif
    }

    static auto getTempCacheDir() -> fs::path {
      return fs::temp_directory_path() / "draconis++";
    }

    explicit CacheManager(types::Option<fs::path> directory = {})
      : m_globalPolicy { .location = CacheLocation::Persistent, .ttl = seconds(60) }, m_directory(std::move(directory)) {}

    static auto isValidKey(const types::String& key) -> bool {
      const fs::path path(key);
      return !key.empty() && key != "." && key != ".." &&
        key.find_first_of("/\\:\0", 0, 4) == types::String::npos && !path.has_root_path() && !path.has_parent_path();
    }

    template <typename T>
    auto get(const types::String& key, types::Option<CachePolicy> policy = {}) -> types::Option<T> {
      if (!isValidKey(key))
        return types::None;
      auto result = getOrSet<T>(key, policy, []() -> types::Result<T> {
        return types::Err(error::DracError(error::DracErrorCode::NotFound, "Cache miss"));
      });
      if (result)
        return std::move(*result);
      return types::None;
    }

    template <typename T>
    auto set(const types::String& key, const T& value, const CachePolicy& policy) -> void {
      if (m_ignoreCache->load(std::memory_order_relaxed) || !isValidKey(key))
        return;
      invalidate(key);
      (void)getOrSet<T>(key, policy, [&]() -> types::Result<T> { return value; });
    }

    auto setGlobalPolicy(const CachePolicy& policy) -> types::Unit {
      types::LockGuard lock(m_cacheMutex);
      m_globalPolicy = policy;
      m_inMemoryCache.clear();
      m_inFlight.clear();
      ++m_globalGeneration;
    }

    template <typename T>
    struct CacheEntry {
      T                         data;
      types::u64                created = 0;
      types::u32                schema  = 2;
      types::Option<types::u64> expires; // store as UNIX timestamp (seconds since epoch), None if no expiry
    };

    struct MemoryCacheEntry {
      std::any                 data;
      system_clock::time_point expires;
      system_clock::time_point created;
    };

    struct InFlightEntry {
      std::condition_variable completedCondition;
      std::any                outcome;
      bool                    completed = false;
    };

    template <typename T, typename Fetcher>
    auto getOrSet(
      const types::String&       key,
      types::Option<CachePolicy> overridePolicy,
      Fetcher&&                  fetcher
    ) -> types::Result<T> {
      if constexpr (DRAC_ENABLE_CACHING) {
        if (m_ignoreCache->load(std::memory_order_relaxed) || !isValidKey(key))
          return fetcher();

        static_assert(std::copy_constructible<T>, "Cached values must be copy constructible");

        CachePolicy                    policy;
        types::u64                     keyGeneration    = 0;
        types::u64                     globalGeneration = 0;
        std::shared_ptr<InFlightEntry> inFlight;

        // Claim this key's fetch slot. Waiters share the leader's typed result.
        for (;;) {
          std::unique_lock lock(m_cacheMutex);
          policy = overridePolicy.value_or(m_globalPolicy);

          if (const auto iter = m_inMemoryCache.find(key); iter != m_inMemoryCache.end()) {
            const auto expiry = policy.ttl ? std::min(iter->second.expires, iter->second.created + *policy.ttl) : iter->second.expires;
            if (system_clock::now() >= expiry) {
              m_inMemoryCache.erase(iter);
            } else if (const auto* value = std::any_cast<T>(&iter->second.data)) {
              return *value;
            } else {
              m_inMemoryCache.erase(iter);
            }
          }

          if (const auto iter = m_inFlight.find(key); iter != m_inFlight.end()) {
            inFlight = iter->second;
            inFlight->completedCondition.wait(lock, [&inFlight] { return inFlight->completed; });

            if (const auto* outcome = std::any_cast<types::Result<T>>(&inFlight->outcome))
              return *outcome;

            // A simultaneous caller used this key with a different type.
            continue;
          }

          inFlight = std::make_shared<InFlightEntry>();
          m_inFlight.emplace(key, inFlight);
          keyGeneration    = getKeyGenerationLocked(key);
          globalGeneration = m_globalGeneration;
          break;
        }

        auto complete = [this, &key, &inFlight](const types::Result<T>* outcome) {
          completeInFlight(key, inFlight, outcome);
        };

        try {
          const types::Option<fs::path> filePath = getCacheFilePath(key, policy.location);

          if (filePath) {
            std::error_code existsError;
            if (fs::exists(*filePath, existsError) && !existsError) {
              bool validDiskEntry = false;

              if (types::Option<types::String> fileContents = readCacheFile(*filePath)) {
                CacheEntry<T> entry;
                if (glz::read_beve(entry, *fileContents) == glz::error_code::none) {
                  const system_clock::time_point created = system_clock::time_point(seconds(entry.created));
                  system_clock::time_point       expiry  = entry.expires.has_value()
                    ? system_clock::time_point(seconds(*entry.expires))
                    : system_clock::time_point::max();

                  if (policy.ttl)
                    expiry = std::min(expiry, created + *policy.ttl);
                  if (entry.schema == 2 && entry.created > 0 && system_clock::now() < expiry) {
                    validDiskEntry              = true;
                    types::Result<T> diskResult = entry.data;
                    publishMemoryIfCurrent(key, entry.data, expiry, created, keyGeneration, globalGeneration);
                    complete(&diskResult);
                    return diskResult;
                  }
                }
              }

              if (!validDiskEntry) {
                std::error_code removeError;
                fs::remove(*filePath, removeError);
              }
            }
          }

          types::Result<T> fetchedResult = fetcher();

          if (!fetchedResult) {
            complete(&fetchedResult);
            return fetchedResult;
          }

          const auto                created = system_clock::now();
          types::Option<types::u64> expiryTimestamp;
          system_clock::time_point  expiry = system_clock::time_point::max();
          if (policy.ttl.has_value()) {
            expiry          = created + *policy.ttl;
            expiryTimestamp = duration_cast<seconds>(expiry.time_since_epoch()).count();
          }

          const bool published = publishMemoryIfCurrent(key, *fetchedResult, expiry, created, keyGeneration, globalGeneration);

          if (published && policy.location != CacheLocation::InMemory && filePath) {
            CacheEntry<T> newEntry {
              .data    = *fetchedResult,
              .created = static_cast<types::u64>(duration_cast<seconds>(created.time_since_epoch()).count()),
              .schema  = 2,
              .expires = expiryTimestamp
            };

            types::String binaryBuffer;
            glz::write_beve(newEntry, binaryBuffer);
            writeCacheFileAtomically(*filePath, binaryBuffer, key, keyGeneration, globalGeneration);
          }

          complete(&fetchedResult);
          return fetchedResult;
        } catch (...) {
          complete(nullptr);
          throw;
        }
      } else {
        (void)key;
        (void)overridePolicy;
        return fetcher();
      }
    }

    template <typename T, typename Fetcher>
    auto getOrSet(const types::String& key, Fetcher&& fetcher) -> types::Result<T> {
      return getOrSet<T>(key, types::None, std::forward<Fetcher>(fetcher));
    }

    /**
     * @brief Remove a cached entry corresponding to the given key.
     *
     * This erases the entry from the in-memory cache and also attempts to
     * remove any corresponding files in the temporary and persistent cache
     * locations (if they exist).
     *
     * @param key Cache key to invalidate.
     */
    auto invalidate(const types::String& key) -> types::Unit {
      if constexpr (DRAC_ENABLE_CACHING) {
        types::LockGuard lock(m_cacheMutex);
        m_inMemoryCache.erase(key);
        // Existing waiters retain their flight; new callers claim a fresh one.
        m_inFlight.erase(key);
        ++m_keyGenerations[key];

        for (const CacheLocation loc : { CacheLocation::TempDirectory, CacheLocation::Persistent })
          if (const types::Option<fs::path> filePath = getCacheFilePath(key, loc); filePath) {
            std::error_code errc;
            fs::remove(*filePath, errc);
          }
      } else {
        (void)key;
      }
    }

    /**
     * @brief Remove **all** cached data – both in-memory and on-disk.
     *
     * This clears the in-memory cache map and removes all files from the
     * persistent and temporary cache directories while preserving the
     * directory structure.
     */
    auto invalidateAll(bool logRemovals = false) -> types::usize {
      if constexpr (DRAC_ENABLE_CACHING) {
        types::LockGuard lock(m_cacheMutex);
        m_inMemoryCache.clear();
        m_inFlight.clear();
        m_keyGenerations.clear();
        ++m_globalGeneration;

        return removeCacheFiles(m_directory.value_or(getPersistentCacheDir()), logRemovals, "persistent") +
          removeCacheFiles(m_directory.value_or(getTempCacheDir()), logRemovals, "temporary");
      } else {
        (void)logRemovals;
        return 0;
      }
    }

   private:
    // Declared first: destroyed after typed entries and in-flight outcomes.
    std::vector<std::shared_ptr<void>> m_moduleLeases;
    // Observe the flag in the module that created this service, including when
    // getOrSet<T> is instantiated in a separately loaded plugin DLL.
    const std::atomic_bool* m_ignoreCache = &ignoreCache;
    CachePolicy             m_globalPolicy;
    types::Option<fs::path> m_directory;

    // Typed values avoid deserializing BEVE data on every in-memory hit.
    types::UnorderedMap<types::String, MemoryCacheEntry>               m_inMemoryCache;
    types::UnorderedMap<types::String, std::shared_ptr<InFlightEntry>> m_inFlight;
    types::UnorderedMap<types::String, types::u64>                     m_keyGenerations;
    types::u64                                                         m_globalGeneration = 0;

    types::Mutex m_cacheMutex;

    static inline std::atomic<types::u64> m_tempFileCounter = 0;

    auto getKeyGenerationLocked(const types::String& key) const -> types::u64 {
      if (const auto iter = m_keyGenerations.find(key); iter != m_keyGenerations.end())
        return iter->second;
      return 0;
    }

    auto isGenerationCurrentLocked(
      const types::String& key,
      const types::u64     keyGeneration,
      const types::u64     globalGeneration
    ) const -> bool {
      return m_globalGeneration == globalGeneration && getKeyGenerationLocked(key) == keyGeneration;
    }

    template <typename T>
    auto publishMemoryIfCurrent(
      const types::String&           key,
      const T&                       value,
      const system_clock::time_point expiry,
      const system_clock::time_point created,
      const types::u64               keyGeneration,
      const types::u64               globalGeneration
    ) -> bool {
      types::LockGuard lock(m_cacheMutex);
      if (!isGenerationCurrentLocked(key, keyGeneration, globalGeneration))
        return false;

      m_inMemoryCache.insert_or_assign(key, MemoryCacheEntry { .data = value, .expires = expiry, .created = created });
      return true;
    }

    template <typename T>
    auto completeInFlight(
      const types::String&                  key,
      const std::shared_ptr<InFlightEntry>& inFlight,
      const types::Result<T>*               outcome
    ) -> types::Unit {
      {
        types::LockGuard lock(m_cacheMutex);
        if (outcome)
          inFlight->outcome = *outcome;
        inFlight->completed = true;

        if (const auto iter = m_inFlight.find(key); iter != m_inFlight.end() && iter->second == inFlight)
          m_inFlight.erase(iter);
      }
      inFlight->completedCondition.notify_all();
    }

    static auto readCacheFile(const fs::path& path) -> types::Option<types::String> {
      std::ifstream stream(path, std::ios::binary | std::ios::ate);
      if (!stream)
        return types::None;

      const std::streampos endPosition = stream.tellg();
      if (endPosition < 0 || endPosition > 16 * 1024 * 1024)
        return types::None;

      types::String contents(static_cast<types::usize>(endPosition), '\0');
      stream.seekg(0, std::ios::beg);
      if (!contents.empty() && !stream.read(contents.data(), static_cast<std::streamsize>(contents.size())))
        return types::None;

      return contents;
    }

    static auto makeTemporaryFilePath(const fs::path& path) -> fs::path {
      const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
      const auto threadId  = std::hash<std::thread::id> {}(std::this_thread::get_id());
      const auto counter   = m_tempFileCounter.fetch_add(1, std::memory_order_relaxed);

      fs::path temporaryPath = path;
      temporaryPath += std::format(".tmp.{}.{}.{}", timestamp, threadId, counter);
      return temporaryPath;
    }

    auto writeCacheFileAtomically(
      const fs::path&      path,
      const types::String& contents,
      const types::String& key,
      const types::u64     keyGeneration,
      const types::u64     globalGeneration
    ) -> bool {
      std::error_code error;
      fs::create_directories(path.parent_path(), error);
      if (error)
        return false;

      const fs::path temporaryPath  = makeTemporaryFilePath(path);
      bool           writeSucceeded = false;
      {
        std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!stream)
          return false;

        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        stream.flush();
        writeSucceeded = stream.good();
      }

      if (!writeSucceeded) {
        fs::remove(temporaryPath, error);
        return false;
      }

      bool replaced = false;
      {
        types::LockGuard lock(m_cacheMutex);
        if (isGenerationCurrentLocked(key, keyGeneration, globalGeneration)) {
#ifdef _WIN32
          replaced = MoveFileExW(
                       temporaryPath.c_str(),
                       path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
                     ) != 0;
#else
          fs::rename(temporaryPath, path, error);
          replaced = !error;
#endif
        }
      }

      if (!replaced)
        fs::remove(temporaryPath, error);

      return replaced;
    }

    static auto removeCacheFiles(
      const fs::path&         directory,
      const bool              logRemovals,
      const types::StringView locationName
    ) -> types::usize {
      std::error_code error;
      if (!fs::exists(directory, error) || error)
        return 0;

      types::usize removedCount = 0;
      for (const fs::directory_entry& entry : fs::recursive_directory_iterator(directory, error)) {
        std::error_code fileError;
        if (!entry.is_regular_file(fileError) || fileError)
          continue;

        const fs::path& path = entry.path();
        if (fs::remove(path, fileError) && !fileError) {
          ++removedCount;
          if (logRemovals)
            info_log("Removed {} cache file: {}", locationName, path.string());
        }
      }
      return removedCount;
    }

    auto getCacheFilePath(const types::String& key, const CacheLocation location) const -> types::Option<fs::path> {
      if (location == CacheLocation::InMemory)
        return types::None;

      const fs::path keyPath(key);
      if (!isValidKey(key))
        return types::None;

      if (m_directory)
        return *m_directory / keyPath;

      if (location == CacheLocation::TempDirectory)
        return types::Some(getTempCacheDir() / keyPath);

      if (location == CacheLocation::Persistent)
        return types::Some(getPersistentCacheDir() / keyPath);

      return types::None;
    }
  };
} // namespace draconis::utils::cache

namespace glz {
  template <>
  struct meta<draconis::utils::types::OSInfo> {
    using T = draconis::utils::types::OSInfo;

    // clang-format off
    static constexpr detail::Object value = object(
      "name", &T::name,
      "version", &T::version,
      "id", &T::id
    );
    // clang-format on
  };

  template <>
  struct meta<draconis::utils::types::CPUCores> {
    using T = draconis::utils::types::CPUCores;

    static constexpr detail::Object value = object("physical", &T::physical, "logical", &T::logical);
  };

  template <>
  struct meta<draconis::utils::types::NetworkInterface> {
    using T = draconis::utils::types::NetworkInterface;

    // clang-format off
    static constexpr detail::Object value = object(
      "name",        &T::name,
      "isUp",        &T::isUp,
      "isLoopback",  &T::isLoopback,
      "ipv4Address", &T::ipv4Address,
      "macAddress",  &T::macAddress
    );
    // clang-format on
  };

  template <>
  struct meta<draconis::utils::types::DisplayInfo> {
    using T = draconis::utils::types::DisplayInfo;

    // clang-format off
    static constexpr detail::Object value = object(
      "id",          &T::id,
      "resolution",  &T::resolution,
      "refreshRate", &T::refreshRate,
      "isPrimary",   &T::isPrimary
    );
    // clang-format on
  };

  template <>
  struct meta<draconis::utils::types::DisplayInfo::Resolution> {
    using T = draconis::utils::types::DisplayInfo::Resolution;

    static constexpr detail::Object value = object("width", &T::width, "height", &T::height);
  };

  template <typename Tp>
  struct meta<draconis::utils::cache::CacheManager::CacheEntry<Tp>> {
    using T = draconis::utils::cache::CacheManager::CacheEntry<Tp>;

    static constexpr detail::Object value = object("data", &T::data, "created", &T::created, "schema", &T::schema, "expires", &T::expires);
  };
} // namespace glz
