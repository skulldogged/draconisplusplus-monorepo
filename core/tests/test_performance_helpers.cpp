#include <atomic>
#include <barrier>
#include <boost/ut.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <Drac++/Core/Plugin.hpp>
#include <Drac++/Services/Packages.hpp>

#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Env.hpp>
#include <Drac++/Utils/Logging.hpp>

namespace {
  auto UniqueCacheKey(const std::string_view prefix) -> std::string {
    return std::format("{}_{}", prefix, std::chrono::steady_clock::now().time_since_epoch().count());
  }
} // namespace

auto main() -> int {
  using namespace boost::ut;
  using namespace draconis::core::plugin;
  using namespace draconis::utils::cache;
  using namespace draconis::utils::logging;
  using namespace draconis::utils::types;

  "Disabled logs do not evaluate arguments"_test = [] -> void {
    const LogLevel previousLevel = GetRuntimeLogLevel();
    SetRuntimeLogLevel(LogLevel::Info);

    i32 evaluations = 0;
    debug_log("Evaluation count: {}", ++evaluations);

    expect(evaluations == 0_i);
    SetRuntimeLogLevel(previousLevel);
  };

  "Nested plugin fields format in one pass"_test = [] -> void {
    PluginFieldArray values;
    values.emplace_back(i64 { 42 });
    values.emplace_back(String { "value" });

    PluginFieldObject object;
    object.emplace("items", std::move(values));

    const PluginFieldValue value { std::move(object) };
    expect(PluginFieldToString(value) == "items: 42, value");
  };

  "Cache accepts move-only fetchers"_test = [] -> void {
    CacheManager cache;
    auto         fetcher = [value = std::make_unique<i32>(7)]() -> Result<i32> {
      return *value;
    };

    const Result<i32> result = cache.getOrSet<i32>("move_only_fetcher", CachePolicy::inMemory(), std::move(fetcher));
    expect(result.has_value());
    expect(*result == 7_i);
  };

  if constexpr (DRAC_ENABLE_CACHING) {
    "Typed memory cache reuses values without refetching"_test = [] -> void {
      CacheManager cache;
      i32          fetchCount = 0;

      auto fetch = [&fetchCount]() -> Result<String> {
        ++fetchCount;
        return String { "cached" };
      };

      const Result<String> first  = cache.getOrSet<String>("typed_memory_hit", CachePolicy::inMemory(), fetch);
      const Result<String> second = cache.getOrSet<String>("typed_memory_hit", CachePolicy::inMemory(), fetch);

      expect(first.has_value());
      expect(second.has_value());
      expect(*second == "cached");
      expect(fetchCount == 1_i);
    };

    "Typed memory cache evicts type mismatches"_test = [] -> void {
      CacheManager cache;
      i32          stringFetchCount = 0;

      expect(cache.getOrSet<i32>("typed_memory_mismatch", CachePolicy::inMemory(), []() -> Result<i32> {
                    return 42;
                  })
               .has_value());

      const Result<String> result = cache.getOrSet<String>("typed_memory_mismatch", CachePolicy::inMemory(), [&stringFetchCount]() -> Result<String> {
        ++stringFetchCount;
        return String { "replacement" };
      });

      expect(result.has_value());
      expect(*result == "replacement");
      expect(stringFetchCount == 1_i);
    };

    "Different cache keys fetch concurrently"_test = [] -> void {
      CacheManager cache;
      std::barrier fetchBarrier(2);
      const auto   policy     = CachePolicy::inMemory();
      const auto   firstFetch = [&fetchBarrier]() -> Result<i32> {
        fetchBarrier.arrive_and_wait();
        return 1;
      };
      const auto secondFetch = [&fetchBarrier]() -> Result<i32> {
        fetchBarrier.arrive_and_wait();
        return 2;
      };

      auto first  = std::async(std::launch::async, [&] -> types::Result<int> { return cache.getOrSet<i32>("concurrent_key_a", policy, firstFetch); });
      auto second = std::async(std::launch::async, [&] -> types::Result<int> { return cache.getOrSet<i32>("concurrent_key_b", policy, secondFetch); });

      expect(*first.get() == 1_i);
      expect(*second.get() == 2_i);
    };

    "Concurrent misses for one key share one fetch"_test = [] -> void {
      CacheManager                          cache;
      std::atomic<i32>                      fetchCount = 0;
      std::barrier                          startBarrier(8);
      std::vector<std::future<Result<i32>>> futures;
      futures.reserve(8);

      for (i32 index = 0; index < 8; ++index)
        futures.emplace_back(std::async(std::launch::async, [&]() -> Result<i32> {
          startBarrier.arrive_and_wait();
          return cache.getOrSet<i32>("single_flight", CachePolicy::inMemory(), [&]() -> Result<i32> {
            ++fetchCount;
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            return 99;
          });
        }));

      for (auto& future : futures)
        expect(*future.get() == 99_i);
      expect(fetchCount.load() == 1_i);
    };

    "Invalidation during fetch prevents stale publication"_test = [] -> void {
      CacheManager       cache;
      std::promise<void> fetchStarted;
      std::promise<void> allowFetchToFinish;
      auto               finishSignal = allowFetchToFinish.get_future().share();

      auto activeFetch = std::async(std::launch::async, [&]() -> Result<String> {
        return cache.getOrSet<String>("invalidate_during_fetch", CachePolicy::inMemory(), [&]() -> Result<String> {
          fetchStarted.set_value();
          finishSignal.wait();
          return String { "stale" };
        });
      });

      fetchStarted.get_future().wait();
      cache.invalidate("invalidate_during_fetch");
      allowFetchToFinish.set_value();
      expect(*activeFetch.get() == "stale");

      i32        freshFetchCount = 0;
      const auto fresh           = cache.getOrSet<String>("invalidate_during_fetch", CachePolicy::inMemory(), [&]() -> Result<String> {
        ++freshFetchCount;
        return String { "fresh" };
      });
      expect(*fresh == "fresh");
      expect(freshFetchCount == 1_i);
    };

    "Temporary disk entries promote into typed memory"_test = [] -> void {
      const String key = UniqueCacheKey("disk_promotion");
      CacheManager firstCache;
      firstCache.invalidate(key);

      const auto first = firstCache.getOrSet<String>(key, CachePolicy::tempDirectory(), []() -> Result<String> {
        return String { "from_disk" };
      });
      expect(*first == "from_disk");
      expect(std::filesystem::exists(CacheManager::getTempCacheDir() / key));

      CacheManager secondCache;
      i32          fallbackFetchCount = 0;
      const auto   second             = secondCache.getOrSet<String>(key, CachePolicy::tempDirectory(), [&]() -> Result<String> {
        ++fallbackFetchCount;
        return String { "fallback" };
      });
      expect(*second == "from_disk");
      expect(fallbackFetchCount == 0_i);

      secondCache.invalidate(key);
    };

    "Corrupt disk entries are replaced atomically"_test = [] -> void {
      const String key       = UniqueCacheKey("corrupt_entry");
      const auto   cacheDir  = CacheManager::getTempCacheDir();
      const auto   cacheFile = cacheDir / key;
      std::filesystem::create_directories(cacheDir);
      {
        std::ofstream corruptFile(cacheFile, std::ios::binary | std::ios::trunc);
        corruptFile << "not-beve";
      }

      CacheManager cache;
      const auto   repaired = cache.getOrSet<String>(key, CachePolicy::tempDirectory(), []() -> Result<String> {
        return String { "repaired" };
      });
      expect(*repaired == "repaired");

      bool hasTemporaryFile = false;
      for (const auto& entry : std::filesystem::directory_iterator(cacheDir))
        if (entry.path().filename().string().starts_with(key + ".tmp."))
          hasTemporaryFile = true;
      expect(!hasTemporaryFile);

      CacheManager verificationCache;
      i32          fallbackFetchCount = 0;
      const auto   verified           = verificationCache.getOrSet<String>(key, CachePolicy::tempDirectory(), [&]() -> Result<String> {
        ++fallbackFetchCount;
        return String { "fallback" };
      });
      expect(*verified == "repaired");
      expect(fallbackFetchCount == 0_i);
      verificationCache.invalidate(key);
    };

    "Cache invalidation never removes unrelated temp files"_test = [] -> void {
      const String sentinelName = UniqueCacheKey("draconis_unrelated_sentinel");
      const auto   sentinelPath = std::filesystem::temp_directory_path() / sentinelName;
      {
        std::ofstream sentinel(sentinelPath, std::ios::trunc);
        sentinel << "keep";
      }

      const String key = UniqueCacheKey("invalidate_all_owned");
      CacheManager cache;
      expect(cache.getOrSet<String>(key, CachePolicy::tempDirectory(), []() -> Result<String> {
                    return String { "owned" };
                  })
               .has_value());

      const usize removedCount = cache.invalidateAll();
      expect(removedCount >= 1_u);
      expect(std::filesystem::exists(sentinelPath));
      expect(!std::filesystem::exists(CacheManager::getTempCacheDir() / key));

      std::error_code error;
      std::filesystem::remove(sentinelPath, error);
    };

    "Expired memory entries refetch"_test = [] -> void {
      CacheManager      cache;
      i32               fetchCount = 0;
      const CachePolicy immediateExpiry {
        .location = CacheLocation::InMemory,
        .ttl      = std::chrono::seconds(0)
      };

      auto fetch = [&]() -> Result<i32> {
        return ++fetchCount;
      };

      expect(cache.getOrSet<i32>("immediate_expiry", immediateExpiry, fetch).has_value());
      expect(cache.getOrSet<i32>("immediate_expiry", immediateExpiry, fetch).has_value());
      expect(fetchCount == 2_i);
    };

    "Expired disk entries are removed and refetched"_test = [] -> void {
      const String      key = UniqueCacheKey("expired_disk_entry");
      const CachePolicy immediateExpiry {
        .location = CacheLocation::TempDirectory,
        .ttl      = std::chrono::seconds(0)
      };

      CacheManager firstCache;
      firstCache.invalidate(key);
      expect(firstCache.getOrSet<String>(key, immediateExpiry, []() -> Result<String> {
                         return String { "expired" };
                       })
               .has_value());

      CacheManager secondCache;
      i32          fetchCount  = 0;
      const auto   replacement = secondCache.getOrSet<String>(key, immediateExpiry, [&]() -> Result<String> {
        ++fetchCount;
        return String { "replacement" };
      });
      expect(*replacement == "replacement");
      expect(fetchCount == 1_i);
      secondCache.invalidate(key);
    };

    "Cache keys cannot escape cache directories"_test = [] -> void {
      const String    filename    = UniqueCacheKey("path_escape");
      const String    unsafeKey   = "../" + filename;
      const auto      escapedPath = CacheManager::getTempCacheDir().parent_path() / filename;
      std::error_code error;
      std::filesystem::remove(escapedPath, error);

      CacheManager cache;
      const auto   result = cache.getOrSet<String>(unsafeKey, CachePolicy::tempDirectory(), []() -> Result<String> {
        return String { "memory_only" };
      });

      expect(*result == "memory_only");
      expect(!std::filesystem::exists(escapedPath));
    };

    "Invalidation detaches an existing fetch before it completes"_test = [] {
      for (const bool all : { false, true }) {
        CacheManager       cache;
        std::promise<void> entered, release;
        auto               released = release.get_future().share();
        auto               first    = std::async(std::launch::async, [&] {
          return cache.getOrSet<String>("flight_generation", CachePolicy::inMemory(), [&]() -> Result<String> {
            entered.set_value();
            released.wait();
            return "old";
          });
        });
        entered.get_future().wait();
        if (all)
          (void)cache.invalidateAll();
        else
          cache.invalidate("flight_generation");
        auto second = std::async(std::launch::async, [&] {
          return cache.getOrSet<String>("flight_generation", CachePolicy::inMemory(), []() -> Result<String> { return "fresh"; });
        });
        expect(second.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
        release.set_value();
        expect(*first.get() == "old");
        expect(*second.get() == "fresh");
      }
    };

    "A stricter policy constrains an existing memory hit"_test = [] {
      CacheManager cache;
      expect(cache.getOrSet<i32>("policy_change", CachePolicy::neverExpire(), []() -> Result<i32> { return 1; }).has_value());
      const CachePolicy expireNow { .location = CacheLocation::InMemory, .ttl = std::chrono::seconds(0) };
      const auto        result = cache.getOrSet<i32>("policy_change", expireNow, []() -> Result<i32> { return 2; });
      expect(*result == 2_i);
    };

    "Plugin cache shares bypass and key confinement"_test = [] {
      const auto  directory = CacheManager::getTempCacheDir() / "plugin-view";
      PluginCache cache(directory);
      cache.set("normal", String("value"), 600);
      expect(cache.get<String>("normal") == Option<String>("value"));
      CacheManager::ignoreCache = true;
      expect(!cache.get<String>("normal"));
      cache.set("bypassed", String("not written"), 600);
      CacheManager::ignoreCache = false;
      expect(!cache.get<String>("bypassed"));
      for (const String key : { String("../outside"), String("..\\outside"), String("file:stream"), String("/absolute") }) {
        cache.set(key, String("invalid"), 600);
        expect(!cache.get<String>(key));
      }
      expect(!std::filesystem::exists(directory.parent_path() / "outside"));
    };
  } else {
    "Disabled caching always fetches"_test = [] {
      CacheManager cache;
      i32          calls = 0;
      auto         fetch = [&]() -> Result<i32> { return ++calls; };
      expect(*cache.getOrSet<i32>("disabled", fetch) == 1_i);
      expect(*cache.getOrSet<i32>("disabled", fetch) == 2_i);
    };
  }
#if defined(_WIN32) && DRAC_ENABLE_PACKAGECOUNT
  "Missing Windows package directories return errors without throwing"_test = [] {
    using draconis::utils::env::GetEnv;
    const auto previousScoop      = GetEnv(L"SCOOP");
    const auto previousChocolatey = GetEnv(L"ChocolateyInstall");
    const auto missing            = std::filesystem::temp_directory_path() / UniqueCacheKey("missing_package_directory");
    _wputenv_s(L"SCOOP", missing.c_str());
    _wputenv_s(L"ChocolateyInstall", missing.c_str());
    const bool   ignored = CacheManager::ignoreCache.exchange(true);
    CacheManager cache;
    Result<u64>  scoop { 0 }, chocolatey { 0 };
    bool         threw = false;
    try {
      scoop      = draconis::services::packages::CountScoop(cache);
      chocolatey = draconis::services::packages::CountChocolatey(cache);
    } catch (...) {
      threw = true;
    }
    CacheManager::ignoreCache = ignored;
    _wputenv_s(L"SCOOP", previousScoop ? previousScoop->c_str() : L"");
    _wputenv_s(L"ChocolateyInstall", previousChocolatey ? previousChocolatey->c_str() : L"");
    expect(!threw);
    expect(!scoop && !chocolatey);
  };
#endif
  return 0;
}
