#include <boost/ut.hpp>
#include <draconis_c.h>
#include <future>

#include <Drac++/Core/PluginManager.hpp>
#include <Drac++/Core/StaticPlugins.hpp>

#include "FixturePlugin.hpp"

namespace {
  int  destroyed = 0;
  bool failNext  = false;
  class ReentrantPlugin : public FixturePlugin {
   public:
    ~ReentrantPlugin() override {
      ++destroyed;
    }
    auto initialize(const draconis::core::plugin::PluginContext&, PluginCache&) -> Result<Unit> override {
      (void)draconis::core::plugin::GetPluginManager().listLoadedPlugins();
      if (std::exchange(failNext, false))
        return draconis::utils::types::Err(draconis::utils::error::DracError(draconis::utils::error::DracErrorCode::ApiUnavailable, "fixture retry"));
      ready = true;
      return {};
    }
    auto shutdown() -> Unit override {
      (void)draconis::core::plugin::GetPluginManager().listLoadedPlugins();
      ready = false;
    }
  };
} // namespace

auto main(int argc, char** argv) -> int {
  using namespace boost::ut;
  using namespace draconis::core::plugin;
  auto& manager = GetPluginManager();
  RegisterStaticPlugin("lifecycle_fixture", { []() -> IPlugin* { return new ReentrantPlugin; }, [](IPlugin* value) { delete value; } });
  CacheManager cache;

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
  manager.shutdown();
}
