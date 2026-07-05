#pragma once

#include <Drac++/Core/System.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/PluginManager.hpp>
#endif

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"

namespace draconis::core::system {
  namespace types = ::draconis::utils::types;

  using config::Config;

  using std::chrono::seconds;

#if DRAC_ENABLE_PLUGINS
  using plugin::GetPluginManager;
  using plugin::ISystemInfoPlugin;
#endif

  /**
   * @brief Utility struct for storing system information.
   *
   * @details Performance optimizations:
   * - Plugin data stored in a flat map for O(1) access
   * - Lazy plugin loading only when accessed
   * - Minimal memory allocations
   */
  struct SystemInfo {
    types::Result<types::String>        date;
    types::Result<types::String>        host;
    types::Result<types::String>        kernelVersion;
    types::Result<types::OSInfo>        operatingSystem;
    types::Result<types::ResourceUsage> memInfo;
    types::Result<types::String>        desktopEnv;
    types::Result<types::String>        windowMgr;
    types::Result<types::ResourceUsage> diskUsage;
    types::Result<types::String>        shell;
    types::Result<types::String>        cpuModel;
    types::Result<types::CPUCores>      cpuCores;
    types::Result<types::String>        gpuModel;
    types::Result<seconds>              uptime;
#if DRAC_ENABLE_PACKAGECOUNT
    types::Result<types::u64> packageCount;
#endif

#if DRAC_ENABLE_PLUGINS
    // Plugin-contributed data organized by plugin ID
    plugin::PluginData pluginData;

    struct PluginDisplayInfo {
      types::String                icon;
      types::String                label;
      types::Option<types::String> value = std::nullopt;
      types::Option<types::String> error = std::nullopt;
    };

    // Display data provided by plugins (icon/label/value)
    types::Map<types::String, PluginDisplayInfo> pluginDisplay;

    /**
     * @brief Get plugin-contributed field value
     * @param pluginId ID of the plugin (e.g., "weather")
     * @param fieldName Name of the field to retrieve
     * @return Field value or empty string if not found
     */
    [[nodiscard]] auto getPluginField(const types::String& pluginId, const types::String& fieldName) const noexcept -> types::String {
      if (auto pluginIter = pluginData.find(pluginId); pluginIter != pluginData.end())
        if (auto fieldIter = pluginIter->second.find(fieldName); fieldIter != pluginIter->second.end())
          return plugin::PluginFieldToString(fieldIter->second);

      return {};
    }

    /**
     * @brief Check if plugin field exists
     * @param pluginId ID of the plugin
     * @param fieldName Name of the field to check
     * @return True if field exists
     */
    [[nodiscard]] auto hasPluginField(const types::String& pluginId, const types::String& fieldName) const noexcept -> bool {
      if (auto pluginIter = pluginData.find(pluginId); pluginIter != pluginData.end())
        return pluginIter->second.contains(fieldName);
      return false;
    }

    /**
     * @brief Get all plugin IDs that have contributed data
     * @return Vector of plugin IDs
     */
    [[nodiscard]] auto getPluginIds() const -> types::Vec<types::String> {
      types::Vec<types::String> ids;
      ids.reserve(pluginData.size());
      for (const auto& [pluginId, fields] : pluginData)
        ids.push_back(pluginId);
      return ids;
    }

    /**
     * @brief Get all field names for a specific plugin
     * @param pluginId ID of the plugin
     * @return Vector of field names
     */
    [[nodiscard]] auto getPluginFieldNames(const types::String& pluginId) const -> types::Vec<types::String> {
      types::Vec<types::String> names;
      if (auto pluginIter = pluginData.find(pluginId); pluginIter != pluginData.end()) {
        names.reserve(pluginIter->second.size());
        for (const auto& [name, value] : pluginIter->second)
          names.push_back(name);
      }
      return names;
    }
#endif

    /**
     * @brief Convert all system info to a flat key-value map
     * @details This is the single source of truth for all system info data.
     * Used by compact output, plugins, and any other consumers that need
     * a generic map representation. Adding new fields here automatically
     * makes them available everywhere.
     * @return Map of field names to their string values
     */
    [[nodiscard]] auto toMap() const -> types::Map<types::String, types::String>;

    explicit SystemInfo(utils::cache::CacheManager& cache, const Config& config);

   private:
#if DRAC_ENABLE_PLUGINS
    /**
     * @brief Collect data from all system info plugins efficiently
     * @param cache Cache manager for plugin data persistence
     */
    auto collectPluginData(utils::cache::CacheManager& cache) -> types::Unit;
#endif
  };

} // namespace draconis::core::system
