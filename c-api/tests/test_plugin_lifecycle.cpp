#include <barrier>
#include <boost/ut.hpp>
#include <draconis_c.h>
#include <fstream>
#include <future>

#include <Drac++/Core/PluginManager.hpp>
#include <Drac++/Core/StaticPlugins.hpp>

#include "FixturePlugin.hpp"

namespace {
  int  destroyed     = 0;
  bool failNext      = false;
  int  shutdownCalls = 0;
  bool notReadyNext  = false;
  class ReentrantPlugin : public FixturePlugin {
   public:
    ~ReentrantPlugin() override {
      ++destroyed;
    }
    auto initialize(const draconis::core::plugin::PluginContext&, PluginCache&) -> Result<Unit> override {
      (void)draconis::core::plugin::GetPluginManager().listLoadedPlugins();
      if (std::exchange(failNext, false))
        return draconis::utils::types::Err(draconis::utils::error::DracError(draconis::utils::error::DracErrorCode::ApiUnavailable, "fixture retry"));
      ready = !std::exchange(notReadyNext, false);
      return {};
    }
    auto shutdown() -> Unit override {
      (void)draconis::core::plugin::GetPluginManager().listLoadedPlugins();
      ++shutdownCalls;
      ready = false;
    }
  };

  class DirectoryPlugin : public FixturePlugin {
   public:
    int  initializeCalls = 0;
    auto initialize(const draconis::core::plugin::PluginContext& context, PluginCache&) -> Result<Unit> override {
      ++initializeCalls;
      for (const auto& directory : { context.configDir, context.cacheDir, context.dataDir }) {
        std::ofstream output(directory / "directory-fixture");
        if (!(output << "initialized"))
          return draconis::utils::types::Err(draconis::utils::error::DracError(draconis::utils::error::DracErrorCode::IoError, "missing context directory"));
      }
      ready = true;
      return {};
    }
  };
} // namespace

auto main(int argc, char** argv) -> int {
  using namespace boost::ut;
  using namespace draconis::core::plugin;
  auto& manager = GetPluginManager();
  RegisterStaticPlugin("lifecycle_fixture", { []() -> IPlugin* { return new ReentrantPlugin; }, [](IPlugin* value) { delete value; } });
  CacheManager cache;

  "Context directories exist before initialization and failures are retryable"_test = [&] {
    const auto context  = GetPluginContext();
    auto       loaded   = std::make_shared<LoadedPlugin>();
    auto       instance = std::make_shared<DirectoryPlugin>();
    loaded->instance    = instance;
    loaded->cache       = std::make_shared<PluginCache>(context.cacheDir);
    // The isolated test profile starts without a data directory. A file at
    // that path must produce an I/O error without invoking the plugin.
    fs::create_directories(context.dataDir.parent_path());
    { std::ofstream blocker(context.dataDir); }
    const auto failed = InitializePlugin(loaded);
    expect(!failed);
    if (!failed)
      expect(failed.error().code == draconis::utils::error::DracErrorCode::IoError);
    expect(instance->initializeCalls == 0_i);
    fs::remove(context.dataDir);
    expect(InitializePlugin(loaded).has_value());
    expect(instance->initializeCalls == 1_i);
    for (const auto& directory : { context.configDir, context.cacheDir, context.dataDir })
      expect(fs::is_regular_file(directory / "directory-fixture"));
  };

  const auto discoveryDir = fs::temp_directory_path() / "plugin-discovery-fixture";
  if (argc == 3) {
    fs::create_directories(discoveryDir);
    fs::copy_file(argv[1], discoveryDir / ("discovered_fixture" + fs::path(argv[1]).extension().string()));
    manager.addSearchPath(discoveryDir);
  }
  "Concurrent first initialization publishes complete discovery"_test = [&] {
    expect(argc == 3_i);
    if (argc != 3)
      return;
    for (int round = 0; round < 4; ++round) {
      manager.shutdown();
      std::barrier                   start(16);
      std::vector<std::future<bool>> callers;
      for (int index = 0; index < 16; ++index)
        callers.push_back(std::async(std::launch::async, [&] {
          start.arrive_and_wait();
          if (!manager.initialize())
            return false;
          return manager.createInfoProvider("discovered_fixture").has_value();
        }));
      for (auto& caller : callers)
        expect(caller.get());
    }
    manager.shutdown();
  };

  "Owning snapshots survive unload and direct-load shutdown"_test = [&] {
    manager.shutdown();
    expect(manager.loadPlugin("lifecycle_fixture", cache).has_value());
    auto snapshot = manager.getInfoProviderPlugins();
    expect(snapshot.size() == 1_u);
    expect(manager.unloadPlugin("lifecycle_fixture").has_value());
    expect(destroyed == 0_i);
    {
      const auto lock = snapshot.front().lock();
      expect(snapshot.front()->isReady());
    }
    snapshot.clear();
    expect(destroyed == 1_i);
    expect(manager.loadPlugin("lifecycle_fixture", cache).has_value());
    manager.shutdown();
    expect(destroyed == 2_i);
  };

  "Failed initialization can be retried"_test = [&] {
    failNext = true;
    expect(!manager.loadPlugin("lifecycle_fixture", cache));
    expect(!manager.isPluginLoaded("lifecycle_fixture"));
    expect(manager.loadPlugin("lifecycle_fixture", cache).has_value());
    manager.shutdown();
  };

  "Only successfully initialized instances receive shutdown"_test = [&] {
    manager.shutdown();
    shutdownCalls     = 0;
    const auto before = destroyed;
    expect(manager.getPluginMetadata("lifecycle_fixture").has_value());
    expect(destroyed == before + 1);
    expect(shutdownCalls == 0_i);
    expect(manager.loadPlugin("lifecycle_fixture", cache, {}, [](StringView) { return false; }).has_value());
    expect(destroyed == before + 2);
    expect(shutdownCalls == 0_i);
    failNext = true;
    expect(!manager.loadPlugin("lifecycle_fixture", cache));
    expect(destroyed == before + 3);
    expect(shutdownCalls == 0_i);
    notReadyNext = true;
    expect(!manager.loadPlugin("lifecycle_fixture", cache));
    expect(destroyed == before + 4);
    expect(shutdownCalls == 1_i);
    expect(manager.loadPlugin("lifecycle_fixture", cache).has_value());
    auto retained = manager.getInfoProviderPlugins();
    manager.shutdown();
    expect(shutdownCalls == 1_i);
    retained.clear();
    expect(destroyed == before + 5);
    expect(shutdownCalls == 2_i);
  };

  "C handles retain dynamic modules and serialize operations"_test = [&] {
    expect(argc == 3_i);
    if (argc != 3)
      return;
    expect(DracLoadPluginFromPath(argv[2]) == nullptr);
    auto* nativeCache = DracCreateCacheManager();
    auto* plugin      = DracLoadPluginFromPath(argv[1]);
    expect(plugin != nullptr);
    if (!plugin) {
      DracDestroyCacheManager(nativeCache);
      return;
    }
    expect(!DracPluginIsReady(plugin));
    expect(DracPluginSetConfig(plugin, "throw") == DRAC_ERROR_INTERNAL_ERROR);
    expect(DracPluginInitialize(plugin, nativeCache) == DRAC_SUCCESS);
    expect(DracPluginInitialize(plugin, nativeCache) == DRAC_SUCCESS);
    DracShutdownPluginManager();
    expect(DracPluginIsReady(plugin));
    auto collect = [&] {
      for (int i = 0; i < 100; ++i)
        if (DracPluginCollectData(plugin, nativeCache) != DRAC_SUCCESS)
          return false;
      return true;
    };
    auto first  = std::async(std::launch::async, collect);
    auto second = std::async(std::launch::async, collect);
    expect(first.get());
    expect(second.get());
    CacheManager::ignoreCache = true;
    expect(DracPluginCollectData(plugin, nativeCache) == DRAC_SUCCESS);
    expect(DracPluginCollectData(plugin, nativeCache) == DRAC_SUCCESS);
    CacheManager::ignoreCache = false;
    auto fields               = DracPluginGetFields(plugin);
    expect(fields.count == 2_u);
    for (size_t index = 0; index < fields.count; ++index) {
      const auto& field = fields.items[index];
      if (std::string_view(field.key) == "samples")
        expect(field.value.i64Value == 202_i);
      if (std::string_view(field.key) == "fetches")
        expect(field.value.i64Value == 3_i);
    }
    DracFreePluginFieldList(&fields);
    DracUnloadPlugin(plugin);
    DracDestroyCacheManager(nativeCache);
  };

  "Discovery returns static and dynamic metadata without loading providers"_test = [&] {
    manager.shutdown();
    auto discovered  = DracDiscoverPlugins();
    bool foundStatic = false, foundDynamic = false;
    for (size_t index = 0; index < discovered.count; ++index) {
      const auto&            item = discovered.items[index];
      const std::string_view name(item.name);
      if (name != "lifecycle_fixture" && name != "discovered_fixture")
        continue;
      foundStatic |= name == "lifecycle_fixture";
      foundDynamic |= name == "discovered_fixture";
      expect(item.version && item.author && item.description);
      if (item.version && item.author && item.description) {
        expect(std::string_view(item.version) == "1");
        expect(std::string_view(item.author) == "tests");
        expect(std::string_view(item.description) == "Local fixture");
      }
      expect(!manager.isPluginLoaded(std::string(name)));
    }
    expect(foundStatic && foundDynamic);
    DracFreePluginInfoList(&discovered);
  };

  "Failed manager initialization remains retryable"_test = [&] {
    manager.shutdown();
    const auto blockedPath = discoveryDir / "blocked-search-path";
    { std::ofstream blocker(blockedPath); }
    manager.addSearchPath(blockedPath);
    expect(!manager.initialize());
    expect(!manager.isInitialized());
    expect(!manager.initialize());
    expect(!manager.isInitialized());
    fs::remove(blockedPath);
    fs::create_directory(blockedPath);
    expect(manager.initialize().has_value());
    expect(manager.isInitialized());
  };
  manager.shutdown();
}
