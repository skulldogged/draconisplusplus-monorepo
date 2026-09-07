#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "../Utils/CacheManager.hpp"
#include "Plugin.hpp"
#include "PluginConfig.hpp"

namespace draconis::core::plugin {
  namespace fs = std::filesystem;
  using namespace utils::types;
  using utils::cache::CacheManager;

#if DRAC_ENABLE_PLUGINS
  #ifdef _WIN32
  using DynamicLibraryHandle = HMODULE;
  #else
  using DynamicLibraryHandle = void*;
  #endif

  struct LoadedPlugin {
    // The custom deleter shuts down successfully initialized instances, calls
    // the matching factory destructor, then
    // releases the library. Keep the cache alive through instance destruction.
    std::shared_ptr<PluginCache>       cache;
    std::shared_ptr<std::atomic<bool>> shutdownEligible = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<IPlugin>           instance;
    PluginMetadata                     metadata;
    mutable std::recursive_mutex       mutex;
    bool                               initialized = false;
    bool                               ready       = false;
  };

  // Copyable ownership plus an explicit per-instance operation lock. Retained
  // handles remain usable after manager unload/shutdown. Callers must hold lock()
  // across related calls and copy any returned references before releasing it.
  template <typename T>
  class PluginHandle {
    std::shared_ptr<LoadedPlugin> m_owner;
    T*                            m_instance = nullptr;

   public:
    PluginHandle() = default;
    explicit PluginHandle(std::shared_ptr<LoadedPlugin> owner)
      : m_owner(std::move(owner)), m_instance(dynamic_cast<T*>(m_owner->instance.get())) {}
    explicit operator bool() const {
      return m_instance != nullptr;
    }
    auto operator->() const -> T* {
      return m_instance;
    }
    auto get() const -> T* {
      return m_instance;
    }
    auto lock() const -> std::unique_lock<std::recursive_mutex> {
      return std::unique_lock(m_owner->mutex);
    }
    auto cache() const -> PluginCache& {
      return *m_owner->cache;
    }
    auto initialize() const -> Result<Unit>;
  };

  auto GetPluginContext() -> PluginContext;
  auto InitializePlugin(const std::shared_ptr<LoadedPlugin>& plugin) -> Result<Unit>;
  template <typename T>
  auto PluginHandle<T>::initialize() const -> Result<Unit> {
    return InitializePlugin(m_owner);
  }

  class PluginManager {
    Map<String, std::shared_ptr<LoadedPlugin>> m_plugins;
    Map<String, fs::path>                      m_discoveredPlugins;
    Map<String, u64>                           m_loading;
    Map<String, Pair<PluginType, String>>      m_providerMetadata;
    Vec<fs::path>                              m_pluginSearchPaths;
    mutable std::shared_mutex                  m_mutex;
    std::atomic<bool>                          m_initialized         = false;
    u64                                        m_generation          = 0;
    u64                                        m_discoveryGeneration = 0;
    PluginManager()                                                  = default;
    static auto loadDynamicLibrary(const fs::path&) -> Result<DynamicLibraryHandle>;
    static auto unloadDynamicLibrary(DynamicLibraryHandle) -> Unit;
    static auto getCreatePluginFunc(DynamicLibraryHandle) -> Result<IPlugin* (*)()>;
    static auto getDestroyPluginFunc(DynamicLibraryHandle) -> Result<void (*)(IPlugin*)>;
    static auto syncPluginLogLevel(DynamicLibraryHandle) -> void;
    auto        constructPlugin(const String&, const Option<fs::path>& = {}) -> Result<std::shared_ptr<LoadedPlugin>>;
    auto        scanForPluginsLocked() -> Result<Unit>;

   public:
    PluginManager(const PluginManager&)                    = delete;
    auto operator=(const PluginManager&) -> PluginManager& = delete;
    ~PluginManager() noexcept;
    static auto getInstance() -> PluginManager&;
    auto        initialize(const PluginConfig& config = {}) -> Result<Unit>;
    auto        shutdown() -> Unit;
    auto        isInitialized() const -> bool {
      return m_initialized;
    }
    auto addSearchPath(const fs::path&) -> Unit;
    auto getSearchPaths() const -> Vec<fs::path>;
    auto scanForPlugins() -> Result<Unit>;
    auto loadPlugin(const String&, CacheManager&, Option<PluginType> = {}, const std::function<bool(StringView)>& providerFilter = {}) -> Result<Unit>;
    auto loadPluginsOfType(PluginType, CacheManager&, const std::function<bool(StringView)>& providerFilter = {}) -> Unit;
    auto unloadPlugin(const String&) -> Result<Unit>;
    // Independent constructed handles give C consumers identical static/dynamic
    // configure-before-initialize behavior. Explicit paths never resolve by stem.
    auto createInfoProvider(const String&, Option<fs::path> = {}) -> Result<PluginHandle<IInfoProviderPlugin>>;
    auto getPlugin(const String&) const -> Option<PluginHandle<IPlugin>>;
    auto getInfoProviderPlugins() const -> Vec<PluginHandle<IInfoProviderPlugin>>;
    auto getOutputFormatPlugins() const -> Vec<PluginHandle<IOutputFormatPlugin>>;
    auto getInfoProviderByName(const String&) const -> Option<PluginHandle<IInfoProviderPlugin>>;
    auto getSystemInfoPlugins() const -> Vec<PluginHandle<IInfoProviderPlugin>> {
      return getInfoProviderPlugins();
    }
    auto listLoadedPlugins() const -> Vec<PluginMetadata>;
    auto listDiscoveredPlugins() const -> Vec<String>;
    // Inspect any discovered plugin without initializing or retaining it in the manager.
    auto getPluginMetadata(const String&) -> Result<PluginMetadata>;
    auto isPluginLoaded(const String&) const -> bool;
  };
#else
  class PluginManager {
   public:
    static auto getInstance() -> PluginManager& {
      static PluginManager manager;
      return manager;
    }
    auto initialize(const PluginConfig& = {}) -> Result<Unit> {
      return {};
    }
    auto shutdown() -> Unit {}
    auto isInitialized() const -> bool {
      return false;
    }
  };
#endif
  inline auto GetPluginManager() -> PluginManager& {
    return PluginManager::getInstance();
  }
} // namespace draconis::core::plugin
