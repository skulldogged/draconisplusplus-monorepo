#pragma once

#include <chrono>
#include <filesystem>
#include <glaze/glaze.hpp>
#include <thread>

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
      return { .location = CacheLocation::TempDirectory, .ttl = types::None };
    }
  };

  namespace detail {
    /**
     * @brief One serialized cache entry inside a snapshot file.
     *
     * The expiry is duplicated outside the BEVE payload so entries can be
     * validated and pruned without knowing the payload type.
     */
    struct StoredCacheEntry {
      types::String             data;    ///< BEVE-encoded CacheEntry<T>.
      types::Option<types::u64> expires; ///< UNIX timestamp (seconds), None = never.
    };

    /// On-disk snapshot format: all entries for one cache location.
    using CacheSnapshot = types::Map<types::String, StoredCacheEntry>;
  } // namespace detail

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
    static inline bool ignoreCache = false;

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

    CacheManager()
      : m_globalPolicy { .location = CacheLocation::Persistent, .ttl = days(1) } {}

    CacheManager(const CacheManager&)                    = delete;
    CacheManager(CacheManager&&)                         = delete;
    auto operator=(const CacheManager&) -> CacheManager& = delete;
    auto operator=(CacheManager&&) -> CacheManager&      = delete;

    ~CacheManager() {
      flush();
    }

    auto setGlobalPolicy(const CachePolicy& policy) -> types::Unit {
      types::LockGuard lock(m_cacheMutex);
      m_globalPolicy = policy;
    }

    /**
     * @brief Write any modified cache entries to disk.
     *
     * Each on-disk location is stored as a single snapshot file, so a run
     * costs at most one read at first access and one write here — instead of
     * one file open per key, which is expensive on platforms where every
     * file open is scanned (e.g. Windows Defender).
     *
     * Called automatically on destruction.
     */
    auto flush() -> types::Unit {
      if constexpr (DRAC_ENABLE_CACHING) {
        types::LockGuard lock(m_cacheMutex);
        flushStoreLocked(m_tempStore, CacheLocation::TempDirectory);
        flushStoreLocked(m_persistentStore, CacheLocation::Persistent);
      }
    }

    template <typename T>
    struct CacheEntry {
      T                         data;
      types::Option<types::u64> expires; // store as UNIX timestamp (seconds since epoch), None if no expiry
    };

    template <typename T>
    auto getOrSet(
      const types::String&          key,
      types::Option<CachePolicy>    overridePolicy,
      types::Fn<types::Result<T>()> fetcher
    ) -> types::Result<T> {
      if constexpr (DRAC_ENABLE_CACHING) {
        /* Early-exit if caching is globally disabled for this run. */
        if (ignoreCache)
          return fetcher();

        // The mutex only guards the in-memory stores; fetchers run unlocked
        // so concurrent getOrSet calls for different keys don't serialize on
        // a single global lock. Disk I/O happens once per location: the
        // snapshot is loaded on first access and flushed on destruction.
        CachePolicy policy;

        {
          types::LockGuard lock(m_cacheMutex);

          policy = overridePolicy.value_or(m_globalPolicy);

          Store& store = storeFor(policy.location);
          ensureLoadedLocked(store, policy.location);

          if (const auto iter = store.entries.find(key); iter != store.entries.end() && !IsExpired(iter->second.expires))
            if (CacheEntry<T> entry; glz::read_beve(entry, iter->second.data) == glz::error_code::none)
              return entry.data;
        }

        // Cache miss: call fetcher without holding the lock
        types::Result<T> fetchedResult = fetcher();

        if (!fetchedResult)
          return fetchedResult;

        // Store in cache
        types::Option<types::u64> expiryTs;
        if (policy.ttl.has_value()) {
          system_clock::time_point now        = system_clock::now();
          system_clock::time_point expiryTime = now + *policy.ttl;

          expiryTs = duration_cast<seconds>(expiryTime.time_since_epoch()).count();
        }

        CacheEntry<T> newEntry {
          .data    = *fetchedResult,
          .expires = expiryTs
        };

        std::string binaryBuffer;
        glz::write_beve(newEntry, binaryBuffer);

        {
          types::LockGuard lock(m_cacheMutex);

          Store& store       = storeFor(policy.location);
          store.entries[key] = detail::StoredCacheEntry { std::move(binaryBuffer), expiryTs };

          if (policy.location != CacheLocation::InMemory)
            store.dirty = true;
        }

        return fetchedResult;
      } else {
        (void)key;
        (void)overridePolicy;
        return fetcher();
      }
    }

    template <typename T>
    auto getOrSet(const types::String& key, types::Fn<types::Result<T>()> fetcher) -> types::Result<T> {
      return getOrSet(key, types::None, fetcher);
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

        m_memStore.entries.erase(key);

        for (const CacheLocation loc : { CacheLocation::TempDirectory, CacheLocation::Persistent }) {
          Store& store = storeFor(loc);
          ensureLoadedLocked(store, loc);

          if (store.entries.erase(key) > 0)
            store.dirty = true;

          // Also remove any legacy per-key cache file from older versions.
          if (const types::Option<fs::path> dir = getSnapshotDir(loc)) {
            std::error_code errc;
            fs::remove(*dir / key, errc);
          }
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
    auto invalidateAll(bool logRemovals = false) -> types::u8 {
      if constexpr (DRAC_ENABLE_CACHING) {
        types::LockGuard lock(m_cacheMutex);

        types::u8 removedCount = 0;

        // Record keys currently present so we can clean their (legacy)
        // temp-dir copies after we clear the stores.
        types::Vec<types::String> keys;
        keys.reserve(m_memStore.entries.size() + m_tempStore.entries.size() + m_persistentStore.entries.size());
        for (const Store* store : { &m_memStore, &m_tempStore, &m_persistentStore })
          for (const auto& [key, val] : store->entries)
            keys.emplace_back(key);

        // Clear all in-memory stores. The snapshot files are removed below,
        // so there is nothing left to flush.
        for (Store* store : { &m_memStore, &m_tempStore, &m_persistentStore }) {
          store->entries.clear();
          store->loaded = true;
          store->dirty  = false;
        }

        // Remove all files from persistent cache directory.
        const fs::path persistentDir = getPersistentCacheDir();

        if (fs::exists(persistentDir)) {
          std::error_code errc;
          for (const fs::directory_entry& entry : fs::recursive_directory_iterator(persistentDir, errc))
            if (entry.is_regular_file()) {
              fs::remove(entry.path(), errc);
              removedCount++;
              if (logRemovals)
                info_log("Removed persistent cache file: {}", entry.path().string());
            }
        }

        // Remove all files from temp directory that match our cache pattern.
        const fs::path tempDir = fs::temp_directory_path();
        if (fs::exists(tempDir)) {
          std::error_code errc;
          for (const fs::directory_entry& entry : fs::directory_iterator(tempDir, errc)) {
            if (entry.is_regular_file()) {
              // Check if this file is one of our cache files by looking at known keys
              // or by checking if it's a file we might have created
              bool shouldRemove = false;

              // Remove files that match our known keys
              for (const types::String& key : keys)
                if (entry.path().filename() == key) {
                  shouldRemove = true;
                  break;
                }

              // The snapshot file holding all temp-directory entries.
              if (!shouldRemove && entry.path().filename() == SNAPSHOT_FILENAME)
                shouldRemove = true;

              // Also remove any files that might be orphaned cache files
              // (files that don't have extensions and are likely our cache files)
              if (!shouldRemove && entry.path().extension().empty())
                shouldRemove = true;

              if (shouldRemove) {
                fs::remove(entry.path(), errc);
                removedCount++;
                if (logRemovals)
                  info_log("Removed temp-directory cache file: {}", entry.path().string());
              }
            }
          }
        }

        return removedCount;
      } else {
        (void)logRemovals;
        return 0;
      }
    }

   private:
    static constexpr const char* SNAPSHOT_FILENAME = "draconis_cache.beve";

    /// All entries for one cache location, loaded lazily from its snapshot file.
    struct Store {
      types::UnorderedMap<types::String, detail::StoredCacheEntry> entries;

      bool loaded = false; ///< Snapshot file has been read (or doesn't exist).
      bool dirty  = false; ///< Entries changed since load; flush() rewrites the snapshot.
    };

    CachePolicy m_globalPolicy;

    Store m_memStore { .entries = {}, .loaded = true, .dirty = false };
    Store m_tempStore;
    Store m_persistentStore;

    types::Mutex m_cacheMutex;

    static auto IsExpired(const types::Option<types::u64>& expires) -> bool {
      return expires.has_value() && system_clock::now() >= system_clock::time_point(seconds(*expires));
    }

    auto storeFor(const CacheLocation location) -> Store& {
      if (location == CacheLocation::TempDirectory)
        return m_tempStore;
      if (location == CacheLocation::Persistent)
        return m_persistentStore;
      return m_memStore;
    }

    static auto getSnapshotDir(const CacheLocation location) -> types::Option<fs::path> {
      if (location == CacheLocation::TempDirectory)
        return types::Some(fs::temp_directory_path());

      if (location == CacheLocation::Persistent)
        return types::Some(getPersistentCacheDir());

      return types::None; // In-memory cache has no file
    }

    auto ensureLoadedLocked(Store& store, const CacheLocation location) -> types::Unit {
      if (store.loaded)
        return;

      store.loaded = true;

      const types::Option<fs::path> dir = getSnapshotDir(location);

      if (!dir)
        return;

      const fs::path snapshotPath = *dir / SNAPSHOT_FILENAME;

      std::error_code statErr;
      const auto      fileSize = fs::file_size(snapshotPath, statErr);

      if (statErr || fileSize == 0)
        return;

      std::ifstream ifs(snapshotPath, std::ios::binary);

      if (!ifs)
        return;

      std::string buffer(fileSize, '\0');

      if (!ifs.read(buffer.data(), static_cast<std::streamsize>(fileSize)))
        return;

      detail::CacheSnapshot snapshot;

      if (glz::read_beve(snapshot, buffer) != glz::error_code::none)
        return;

      store.entries.reserve(snapshot.size());

      for (auto& [key, entry] : snapshot) {
        if (IsExpired(entry.expires)) {
          store.dirty = true; // drop expired entries; rewrite on flush
          continue;
        }

        store.entries.emplace(key, std::move(entry));
      }
    }

    auto flushStoreLocked(Store& store, const CacheLocation location) -> types::Unit {
      if (!store.dirty)
        return;

      const types::Option<fs::path> dir = getSnapshotDir(location);

      if (!dir)
        return;

      detail::CacheSnapshot snapshot;

      for (const auto& [key, entry] : store.entries)
        if (!IsExpired(entry.expires))
          snapshot.emplace(key, entry);

      std::string buffer;
      glz::write_beve(snapshot, buffer);

      std::error_code errc;
      fs::create_directories(*dir, errc);

      if (errc)
        return;

      const fs::path snapshotPath = *dir / SNAPSHOT_FILENAME;

      // Write to a temp file and rename so concurrent readers never see a
      // partially-written snapshot.
      fs::path tmpPath = snapshotPath;
      tmpPath += std::format(".tmp{}", std::hash<std::thread::id> {}(std::this_thread::get_id()));

      bool written = false;
      if (std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc); ofs.is_open())
        written = static_cast<bool>(ofs.write(buffer.data(), static_cast<std::streamsize>(buffer.size())));

      if (written) {
        fs::rename(tmpPath, snapshotPath, errc);
        if (errc) {
          fs::remove(tmpPath, errc);
          return;
        }
      } else {
        fs::remove(tmpPath, errc);
        return;
      }

      store.dirty = false;
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

    static constexpr detail::Object value = object("data", &T::data, "expires", &T::expires);
  };

  template <>
  struct meta<draconis::utils::cache::detail::StoredCacheEntry> {
    using T = draconis::utils::cache::detail::StoredCacheEntry;

    static constexpr detail::Object value = object("data", &T::data, "expires", &T::expires);
  };
} // namespace glz
