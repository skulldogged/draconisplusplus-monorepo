#pragma once
#include <stdexcept>

#include <Drac++/Core/Plugin.hpp>

class FixturePlugin : public draconis::core::plugin::IInfoProviderPlugin {
 public:
  using String = draconis::utils::types::String;
  using Unit   = draconis::utils::types::Unit;
  template <typename T>
  using Result = draconis::utils::types::Result<T>;
  bool ready   = false;
  int  samples = 0;
  int  fetches = 0;
  auto getMetadata() const -> const draconis::core::plugin::PluginMetadata& override {
    static const draconis::core::plugin::PluginMetadata metadata {
      .name         = "fixture",
      .version      = "1",
      .author       = "tests",
      .description  = "Local fixture",
      .type         = draconis::core::plugin::PluginType::InfoProvider,
      .dependencies = {},
    };
    return metadata;
  }
  auto initialize(const draconis::core::plugin::PluginContext&, PluginCache&) -> Result<Unit> override {
    ready = true;
    return {};
  }
  auto setConfig(draconis::utils::types::StringView value) -> Result<Unit> override {
    if (value == "throw")
      throw std::runtime_error("fixture error");
    return {};
  }
  auto shutdown() -> Unit override {
    ready = false;
  }
  auto isReady() const -> bool override {
    return ready;
  }
  auto isEnabled() const -> bool override {
    return true;
  }
  auto getProviderId() const -> String override {
    return "fixture";
  }
  auto collectData(PluginCache& cache) -> Result<Unit> override {
    struct CachedSample {
      int value = 7;
    };
    auto result = cache.getOrSet<CachedSample>("sample", 60, [&]() -> Result<CachedSample> { ++fetches; return CachedSample{}; });
    if (!result)
      return std::unexpected(result.error());
    ++samples;
    return {};
  }
  auto getFields() const -> draconis::core::plugin::PluginFields override {
    return {
      { "samples", draconis::utils::types::i64(samples) },
      { "fetches", draconis::utils::types::i64(fetches) }
    };
  }
  auto getDisplayValue() const -> Result<String> override {
    return "fixture";
  }
  auto getDisplayIcon() const -> String override {
    return "";
  }
  auto getDisplayLabel() const -> String override {
    return "Fixture";
  }
  auto getLastError() const -> draconis::utils::types::Option<String> override {
    return {};
  }
};
