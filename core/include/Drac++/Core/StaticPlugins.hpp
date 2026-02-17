/**
 * @file StaticPlugins.hpp
 * @brief Static plugin registry for statically linked plugins
 * @author Draconis++ Team
 * @version 1.0.0
 *
 * @details When static plugins are enabled via the static_plugins meson option,
 * plugins can be statically linked into the binary rather than loaded dynamically
 * at runtime. This provides:
 * - Fully portable single-binary deployment
 * - Faster startup (no dynamic library loading)
 * - Smaller distribution (no separate .dll/.so files needed)
 *
 * Plugins register via DracInitStaticPlugins() which must be called explicitly.
 */

#pragma once

#if DRAC_ENABLE_PLUGINS

  #include "../Utils/Types.hpp"
  #include "Plugin.hpp"

  #include <cstddef>

namespace draconis::core::plugin {
  /**
   * @struct StaticPluginEntry
   * @brief Entry for a statically compiled plugin
   */
  struct StaticPluginEntry {
    IPlugin* (*createFunc)();
    void (*destroyFunc)(IPlugin*);
  };

  /**
   * @brief Get the mutable registry of static plugins (for registration)
   * @return Reference to the map of plugin name -> entry
   */
  auto GetStaticPluginRegistry() -> utils::types::UnorderedMap<utils::types::String, StaticPluginEntry>&;

  /**
   * @brief Register a static plugin (called by DracInitStaticPlugins)
   * @param name The plugin name
   * @param entry The plugin entry to register
   */
  inline auto RegisterStaticPlugin(const char* name, StaticPluginEntry entry) -> void {
    GetStaticPluginRegistry().emplace(name, entry);
  }

  /**
   * @brief Initialize all static plugins
   * @return Number of plugins registered
   * 
   * This function MUST be called before using any static plugins.
   * It calls all DracRegisterPlugin_* functions exported by plugins.
   */
  auto DracInitStaticPlugins() -> std::size_t;

  /**
   * @brief Check if a plugin is available as a static plugin
   * @param name The plugin name to check
   * @return true if the plugin is statically compiled, false otherwise
   */
  auto IsStaticPlugin(const utils::types::String& name) -> bool;

  /**
   * @brief Create an instance of a static plugin
   * @param name The plugin name
   * @return Pointer to the created plugin instance, or nullptr if not found
   */
  auto CreateStaticPlugin(const utils::types::String& name) -> IPlugin*;

  /**
   * @brief Destroy an instance of a static plugin
   * @param name The plugin name
   * @param plugin The plugin instance to destroy
   */
  auto DestroyStaticPlugin(const utils::types::String& name, IPlugin* plugin) -> void;

} // namespace draconis::core::plugin

#endif // DRAC_ENABLE_PLUGINS
