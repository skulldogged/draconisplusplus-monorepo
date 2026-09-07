#include <array>
#include <atomic>
#include <boost/ut.hpp>

#include "../src/CLI/Core/SystemInfo.hpp"
#include "../src/CLI/UI/UI.hpp"

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/StaticPlugins.hpp>

  #include "../../c-api/tests/FixturePlugin.hpp"

namespace {
  using namespace draconis::core::plugin;
  using namespace draconis::utils::types;
  constexpr std::array            PROVIDERS { "probe_with_underscores", "weather", "now_playing", "unrelated" };
  std::array<std::atomic<int>, 4> constructed {};
  std::array<std::atomic<int>, 4> initialized {};
  std::array<std::atomic<int>, 4> collected {};

  template <usize Index>
  class SelectionProvider : public FixturePlugin {
   public:
    SelectionProvider() {
      ++constructed[Index];
    }
    auto getProviderId() const -> String override {
      return PROVIDERS[Index];
    }
    auto initialize(const PluginContext& context, PluginCache& cache) -> Result<Unit> override {
      ++initialized[Index];
      return FixturePlugin::initialize(context, cache);
    }
    auto collectData(PluginCache&) -> Result<Unit> override {
      ++collected[Index];
      return {};
    }
    auto getFields() const -> PluginFields override {
      return {
        { "field_with_underscores", i64 { 7 } }
      };
    }
  };

  template <usize Index>
  auto RegisterFixture(const char* filename) -> void {
    RegisterStaticPlugin(filename, { []() -> IPlugin* { return new SelectionProvider<Index>; }, [](IPlugin* plugin) -> void { delete plugin; } });
  }
} // namespace
#endif

auto main() -> int {
  using namespace boost::ut;
  using namespace draconis::utils::types;
  using draconis::core::system::SystemInfo;
  draconis::config::Config             config {};
  draconis::utils::cache::CacheManager cache;

  "Custom layouts collect only requested core fields and normalize case"_test = [&] {
    config.ui.layout = {
      { .name = "fixture", .rows = { { .key = "DATE" } } }
    };
    const SystemInfo info(cache, config);
    expect(info.date.has_value() && !info.date->empty());
    expect(info.cpuModel.has_value() && info.cpuModel->empty());
    expect(!SystemInfo::needsPlugins(config));
  };

#if defined(_WIN32) || defined(__APPLE__)
  "Custom layouts keep the platform logo without collecting OS information"_test = [&] {
    const SystemInfo info(cache, config);
    expect(info.operatingSystem.has_value() && info.operatingSystem->id.empty());
    const auto withLogo    = draconis::ui::CreateUI(config, info, false);
    const auto withoutLogo = draconis::ui::CreateUI(config, info, true);
    expect(withLogo.size() > withoutLogo.size());
  };
#endif

  "Compact selection takes precedence over the normal layout"_test = [&] {
    const SystemInfo info(cache, config, "literal");
    expect(info.date.has_value() && info.date->empty());
    expect(!SystemInfo::needsPlugins(config, "literal"));
  };

#if DRAC_ENABLE_PLUGINS
  auto& manager = GetPluginManager();
  manager.shutdown();
  (void)DracInitStaticPlugins();
  auto savedRegistry = std::move(GetStaticPluginRegistry());
  GetStaticPluginRegistry().clear();
  RegisterFixture<0>("renamed_fixture");
  RegisterFixture<1>("weather_fixture");
  RegisterFixture<2>("media_fixture");
  RegisterFixture<3>("unrelated_fixture");

  auto reset = [&] {
    manager.shutdown();
    for (auto& count : constructed) count = 0;
    for (auto& count : initialized) count = 0;
    for (auto& count : collected) count = 0;
    expect(manager.initialize().has_value());
  };

  "Provider IDs and field names containing underscores are preserved"_test = [&] {
    reset();
    const SystemInfo info(cache, config, "{plugin_probe_with_underscores_field_with_underscores}");
    expect(info.getPluginField("probe_with_underscores", "field_with_underscores") == "7");
    expect(initialized[0].load() == 1_i && collected[0].load() == 1_i);
    for (usize index = 1; index < PROVIDERS.size(); ++index)
      expect(initialized[index].load() == 0_i && collected[index].load() == 0_i);
  };

  "Compact aliases select their provider even when its filename differs"_test = [&] {
    reset();
    const SystemInfo weather(cache, config, "{weather}");
    expect(weather.toMap().at("weather") == "fixture");
    const SystemInfo playing(cache, config, "{playing}");
    expect(playing.toMap().at("playing") == "fixture");
    expect(collected[1].load() == 1_i && collected[2].load() == 1_i);
    expect(initialized[0].load() == 0_i && initialized[3].load() == 0_i);
    expect(constructed[0].load() == 1_i && constructed[3].load() == 1_i);
  };

  "Layout plugin rows use exact provider IDs and ignore already loaded unrelated providers"_test = [&] {
    reset();
    expect(manager.loadPlugin("unrelated_fixture", cache).has_value());
    config.ui.layout = {
      { .name = "fixture", .rows = { { .key = "plugin.probe_with_underscores.field_with_underscores" } } }
    };
    const SystemInfo info(cache, config);
    expect(info.getPluginField("probe_with_underscores", "field_with_underscores") == "7");
    expect(collected[0].load() == 1_i && collected[3].load() == 0_i);
  };

  "Full collection for doctor and formatter output overrides selection"_test = [&] {
    reset();
    const SystemInfo info(cache, config, "literal", true);
    expect(info.date.has_value() && !info.date->empty());
    for (const auto& count : collected) expect(count.load() == 1_i);
  };

  "Disabling plugins prevents collection even if a manager is initialized"_test = [&] {
    reset();
    config.plugins.enabled = false;
    const SystemInfo info(cache, config, "{weather}");
    expect(!SystemInfo::needsPlugins(config, "{weather}"));
    for (const auto& count : initialized) expect(count.load() == 0_i);
  };

  manager.shutdown();
  GetStaticPluginRegistry() = std::move(savedRegistry);
#endif
}
