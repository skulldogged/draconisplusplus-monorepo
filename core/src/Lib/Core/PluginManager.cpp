/**
 * @file PluginManager.cpp
 * @brief High-performance plugin manager implementation
 * @author Draconis++ Team
 * @version 1.0.0
 */

#if DRAC_ENABLE_PLUGINS

  #include <cstdio>   // std::fputs, std::fputc
  #include <format>   // std::format
  #include <optional> // std::optional
  #include <string>   // std::string

  #include <Drac++/Core/PluginManager.hpp>
  #include <Drac++/Core/StaticPlugins.hpp>

  #include <Drac++/Utils/CacheManager.hpp>
  #include <Drac++/Utils/Env.hpp>
  #include <Drac++/Utils/Error.hpp>
  #include <Drac++/Utils/Logging.hpp>

  #ifdef _WIN32
    #include <windows.h>
  #else
    #include <dlfcn.h> // dlopen, dlsym, dlclose
  #endif

namespace draconis::core::plugin {
  namespace {
    using utils::error::DracErrorCode;
    using utils::types::StringView;
    using enum DracErrorCode;

    // Platform-specific plugin file extension
  #ifdef _WIN32
    constexpr StringView PLUGIN_EXTENSION = ".dll";
  #elifdef __APPLE__
    constexpr StringView PLUGIN_EXTENSION = ".dylib";
  #else
    constexpr StringView PLUGIN_EXTENSION = ".so";
  #endif

    // Default search paths for plugins
    auto GetDefaultPluginPaths() -> const Vec<fs::path>& {
      static const Vec<fs::path> DEFAULT_PLUGIN_PATHS = []() -> Vec<fs::path> {
        using draconis::utils::env::GetEnv;

        Vec<fs::path> paths;

        // Paths from DRAC_PLUGIN_PATH take priority (colon-separated on Unix,
        // semicolon-separated on Windows). Needed on systems without the FHS
        // directories below (e.g. Nix) and handy for plugin development.
  #ifdef _WIN32
        constexpr char pathSeparator = ';';
  #else
        constexpr char pathSeparator = ':';
  #endif
        if (auto result = GetEnv("DRAC_PLUGIN_PATH")) {
          const String& value = *result;
          for (String::size_type start = 0; start <= value.size();) {
            String::size_type end = value.find(pathSeparator, start);
            if (end == String::npos)
              end = value.size();
            if (end > start)
              paths.emplace_back(value.substr(start, end - start));
            start = end + 1;
          }
        }

  #ifdef _WIN32
        if (auto result = GetEnv("LOCALAPPDATA"))
          paths.push_back(fs::path(*result) / "draconis++" / "plugins");

        if (auto result = GetEnv("APPDATA"))
          paths.push_back(fs::path(*result) / "draconis++" / "plugins");

        if (auto result = GetEnv("USERPROFILE"))
          paths.push_back(fs::path(*result) / ".config" / "draconis++" / "plugins");

        paths.push_back(fs::current_path() / "plugins");
  #else
        paths.push_back(fs::path("/usr/local/lib/draconis++/plugins"));
        paths.push_back(fs::path("/usr/lib/draconis++/plugins"));
        paths.push_back(fs::path(getenv("HOME") ? getenv("HOME") : "") / ".local/lib/draconis++/plugins");
        paths.push_back(fs::current_path() / "plugins");
  #endif
        return paths;
      }();

      return DEFAULT_PLUGIN_PATHS;
    }

    // Get the base config directory for draconis++
    auto GetConfigDir() -> fs::path {
  #ifdef _WIN32
      using draconis::utils::env::GetEnv;
      if (auto result = GetEnv("LOCALAPPDATA"))
        return fs::path(*result) / "draconis++";
      if (auto result = GetEnv("USERPROFILE"))
        return fs::path(*result) / ".config" / "draconis++";
      return fs::current_path();
  #else
      if (const char* xdgConfig = getenv("XDG_CONFIG_HOME"))
        return fs::path(xdgConfig) / "draconis++";
      if (const char* home = getenv("HOME"))
        return fs::path(home) / ".config" / "draconis++";
      return fs::current_path();
  #endif
    }

    // Get the cache directory for draconis++
    auto GetCacheDir() -> fs::path {
  #ifdef _WIN32
      using draconis::utils::env::GetEnv;
      if (auto result = GetEnv("LOCALAPPDATA"))
        return fs::path(*result) / "draconis++" / "cache";
      return GetConfigDir() / "cache";
  #else
      if (const char* xdgCache = getenv("XDG_CACHE_HOME"))
        return fs::path(xdgCache) / "draconis++";
      if (const char* home = getenv("HOME"))
        return fs::path(home) / ".cache" / "draconis++";
      return GetConfigDir() / "cache";
  #endif
    }

    // Get the data directory for draconis++
    auto GetDataDir() -> fs::path {
  #ifdef _WIN32
      using draconis::utils::env::GetEnv;
      if (auto result = GetEnv("LOCALAPPDATA"))
        return fs::path(*result) / "draconis++" / "data";
      return GetConfigDir() / "data";
  #else
      if (const char* xdgData = getenv("XDG_DATA_HOME"))
        return fs::path(xdgData) / "draconis++";
      if (const char* home = getenv("HOME"))
        return fs::path(home) / ".local" / "share" / "draconis++";
      return GetConfigDir() / "data";
  #endif
    }
  } // namespace

  auto GetPluginContext() -> PluginContext {
    return PluginContext {
      .configDir = GetConfigDir() / "plugins",
      .cacheDir  = GetCacheDir() / "plugins",
      .dataDir   = GetDataDir() / "plugins",
    };
  }

  PluginManager::~PluginManager() noexcept {
    try {
      shutdown();
    } catch (const std::exception& error) {
      (void)std::fputs("Plugin manager shutdown failed: ", stderr);
      (void)std::fputs(error.what(), stderr);
      (void)std::fputc('\n', stderr);
    } catch (...) {
      (void)std::fputs("Plugin manager shutdown failed with an unknown exception\n", stderr);
    }
  }

  auto PluginManager::getInstance() -> PluginManager& {
    static PluginManager Instance;
    return Instance;
  }

  auto PluginManager::initialize(const PluginConfig& config) -> Result<Unit> {
    (void)DracInitStaticPlugins();
    {
      const std::unique_lock lock(m_mutex);
      if (m_initialized)
        return {};
      if (config.enabled) {
        for (const auto& path : GetDefaultPluginPaths())
          if (std::ranges::find(m_pluginSearchPaths, path) == m_pluginSearchPaths.end())
            m_pluginSearchPaths.push_back(path);
        const auto scanned = scanForPluginsLocked();
        if (!scanned)
          return scanned;
      }
      // Publish only a complete discovery map. Auto-load callbacks below may
      // reenter the manager and must run without its global lock held.
      m_initialized = true;
    }
    if (!config.enabled)
      return {};
    CacheManager cache;
    for (const auto& name : config.autoLoad)
      if (auto result = loadPlugin(name, cache); !result)
        warn_log("Failed to auto-load '{}': {}", name, result.error().message);
    return {};
  }

  auto PluginManager::shutdown() -> Unit {
    Map<String, std::shared_ptr<LoadedPlugin>> retired;
    {
      const std::unique_lock lock(m_mutex);
      retired.swap(m_plugins);
      m_loading.clear();
      m_discoveredPlugins.clear();
      m_providerMetadata.clear();
      ++m_generation;
      ++m_discoveryGeneration;
      m_initialized = false;
    }
    // Destructors and plugin callbacks run outside the manager lock.
  }

  auto PluginManager::addSearchPath(const fs::path& path) -> Unit {
    const std::unique_lock lock(m_mutex);
    if (std::ranges::find(m_pluginSearchPaths, path) == m_pluginSearchPaths.end())
      m_pluginSearchPaths.push_back(path);
  }

  auto PluginManager::getSearchPaths() const -> Vec<fs::path> {
    const std::shared_lock lock(m_mutex);
    return m_pluginSearchPaths;
  }

  auto PluginManager::scanForPlugins() -> Result<Unit> {
    const std::unique_lock lock(m_mutex);
    return scanForPluginsLocked();
  }

  auto PluginManager::scanForPluginsLocked() -> Result<Unit> {
    Map<String, fs::path> discovered;
    std::error_code       error;
    for (const auto& searchPath : m_pluginSearchPaths) {
      const auto status = fs::status(searchPath, error);
      if (error && error != std::errc::no_such_file_or_directory)
        ERR_FMT(IoError, "Cannot inspect plugin directory '{}': {}", searchPath.string(), error.message());
      if (error == std::errc::no_such_file_or_directory || status.type() == fs::file_type::not_found)
        continue;
      if (!fs::is_directory(status))
        ERR_FMT(IoError, "Plugin search path is not a directory: {}", searchPath.string());
      fs::directory_iterator entries(searchPath, error), end;
      while (!error && entries != end) {
        if (entries->is_regular_file(error) && entries->path().extension() == PLUGIN_EXTENSION)
          discovered.try_emplace(entries->path().stem().string(), entries->path());
        entries.increment(error);
      }
      if (error)
        ERR_FMT(IoError, "Cannot scan plugin directory: {}", error.message());
    }
    m_discoveredPlugins.swap(discovered);
    ++m_discoveryGeneration;
    m_providerMetadata.clear();
    return {};
  }

  auto PluginManager::constructPlugin(const String& name, const Option<fs::path>& exactPath) -> Result<std::shared_ptr<LoadedPlugin>> {
    (void)DracInitStaticPlugins();
    auto loaded               = std::make_shared<LoadedPlugin>();
    loaded->cache             = std::make_shared<PluginCache>(GetPluginContext().cacheDir);
    IPlugin* (*create)()      = nullptr;
    void (*destroy)(IPlugin*) = nullptr;
    std::shared_ptr<void> library;
    if (!exactPath && IsStaticPlugin(name)) {
      const auto entry = GetStaticPluginRegistry().at(name);
      create           = entry.createFunc;
      destroy          = entry.destroyFunc;
    } else {
      fs::path path;
      if (exactPath) {
        path = fs::absolute(*exactPath);
      } else {
        const std::shared_lock lock(m_mutex);
        const auto             iter = m_discoveredPlugins.find(name);
        if (iter == m_discoveredPlugins.end())
          ERR_FMT(NotFound, "Plugin '{}' not found", name);
        path = iter->second;
      }
      const auto handle = TRY(loadDynamicLibrary(path));
      library           = std::shared_ptr<void>(handle, [](void* value) {
        unloadDynamicLibrary(static_cast<DynamicLibraryHandle>(value));
      });
  #ifdef _WIN32
      auto abi = reinterpret_cast<unsigned int (*)()>(GetProcAddress(handle, "DracPluginAbiVersion"));
  #else
      auto abi = reinterpret_cast<unsigned int (*)()>(dlsym(handle, "DracPluginAbiVersion"));
  #endif
      if (!abi || abi() != 2)
        ERR(NotSupported, "Plugin ABI mismatch; rebuild the plugin with this SDK and toolchain");
      loaded->cache->retainModule(library);
      create  = TRY(getCreatePluginFunc(handle));
      destroy = TRY(getDestroyPluginFunc(handle));
      syncPluginLogLevel(handle);
    }
    loaded->instance = std::shared_ptr<IPlugin>(create(), [destroy, library, cache = loaded->cache](IPlugin* value) {
      if (!value)
        return;
      try {
        value->shutdown();
      } catch (...) {}
      try {
        destroy(value);
      } catch (...) {}
    });
    if (!loaded->instance)
      ERR_FMT(InternalError, "Failed to create plugin '{}'", name);
    loaded->metadata = loaded->instance->getMetadata();
    return loaded;
  }

  auto PluginManager::createInfoProvider(const String& name, Option<fs::path> exactPath) -> Result<PluginHandle<IInfoProviderPlugin>> {
    auto                              instance = TRY(constructPlugin(name, exactPath));
    PluginHandle<IInfoProviderPlugin> result(instance);
    if (!result)
      ERR(InvalidArgument, "Plugin is not an information provider");
    return result;
  }

  auto PluginManager::loadPlugin(const String& name, CacheManager& /*cache*/, Option<PluginType> requiredType, const std::function<bool(StringView)>& providerFilter) -> Result<Unit> {
    u64                              generation;
    u64                              discoveryGeneration;
    Option<Pair<PluginType, String>> knownProvider;
    {
      const std::unique_lock lock(m_mutex);
      if (m_plugins.contains(name))
        return {};
      if (m_loading.contains(name))
        ERR(ResourceExhausted, "Plugin is already being initialized");
      generation          = m_generation;
      discoveryGeneration = m_discoveryGeneration;
      if (const auto metadata = m_providerMetadata.find(name); metadata != m_providerMetadata.end())
        knownProvider = metadata->second;
      m_loading.emplace(name, generation);
    }
    auto finish = [&] {
      const std::unique_lock lock(m_mutex);
      if (generation == m_generation)
        m_loading.erase(name);
    };
    try {
      // Provider IDs are discovered without initialization and reused on later requests.
      // Run caller predicates outside the manager lock.
      if (knownProvider && ((requiredType && knownProvider->first != *requiredType) || (providerFilter && (knownProvider->first != PluginType::InfoProvider || !providerFilter(knownProvider->second))))) {
        finish();
        return {};
      }
      auto result = constructPlugin(name);
      if (!result) {
        finish();
        return std::unexpected(result.error());
      }
      auto                     instance = std::move(*result);
      Pair<PluginType, String> metadata { instance->metadata.type, {} };
      if (const auto* provider = dynamic_cast<IInfoProviderPlugin*>(instance->instance.get()))
        metadata.second = provider->getProviderId();
      {
        const std::unique_lock lock(m_mutex);
        if (generation == m_generation && discoveryGeneration == m_discoveryGeneration)
          m_providerMetadata.insert_or_assign(name, metadata);
      }
      if (requiredType && instance->metadata.type != *requiredType) {
        finish();
        return {};
      }
      if (providerFilter) {
        if (metadata.first != PluginType::InfoProvider || !providerFilter(metadata.second)) {
          finish();
          return {};
        }
      }
      auto initialized = InitializePlugin(instance);
      if (!initialized) {
        finish();
        return initialized;
      }
      {
        const std::unique_lock lock(m_mutex);
        if (generation != m_generation)
          ERR(ApiUnavailable, "Plugin manager shut down during initialization");
        m_loading.erase(name);
        m_plugins.emplace(name, std::move(instance));
      }
      return {};
    } catch (...) {
      finish();
      throw;
    }
  }

  auto PluginManager::loadPluginsOfType(PluginType type, CacheManager& cache, const std::function<bool(StringView)>& providerFilter) -> Unit {
    for (const auto& name : listDiscoveredPlugins())
      if (auto result = loadPlugin(name, cache, type, providerFilter); !result)
        debug_log("Failed to load '{}': {}", name, result.error().message);
  }

  auto PluginManager::unloadPlugin(const String& name) -> Result<Unit> {
    std::shared_ptr<LoadedPlugin> retired;
    {
      const std::unique_lock lock(m_mutex);
      const auto             iter = m_plugins.find(name);
      if (iter == m_plugins.end())
        ERR_FMT(NotFound, "Plugin '{}' is not loaded", name);
      retired = std::move(iter->second);
      m_plugins.erase(iter);
      m_providerMetadata.erase(name);
    }
    return {};
  }

  auto PluginManager::getPlugin(const String& name) const -> Option<PluginHandle<IPlugin>> {
    const std::shared_lock lock(m_mutex);
    if (const auto iter = m_plugins.find(name); iter != m_plugins.end())
      return PluginHandle<IPlugin>(iter->second);
    return std::nullopt;
  }

  auto PluginManager::getInfoProviderPlugins() const -> Vec<PluginHandle<IInfoProviderPlugin>> {
    const std::shared_lock                 lock(m_mutex);
    Vec<PluginHandle<IInfoProviderPlugin>> result;
    for (const auto& [name, loaded] : m_plugins)
      if (loaded->ready && loaded->metadata.type == PluginType::InfoProvider) {
        PluginHandle<IInfoProviderPlugin> handle(loaded);
        if (handle)
          result.push_back(std::move(handle));
      }
    return result;
  }

  auto PluginManager::getInfoProviderByName(const String& name) const -> Option<PluginHandle<IInfoProviderPlugin>> {
    for (const auto& handle : getInfoProviderPlugins()) {
      const auto lock = handle.lock();
      if (handle->getProviderId() == name)
        return handle;
    }
    return std::nullopt;
  }

  auto PluginManager::getOutputFormatPlugins() const -> Vec<PluginHandle<IOutputFormatPlugin>> {
    const std::shared_lock                 lock(m_mutex);
    Vec<PluginHandle<IOutputFormatPlugin>> result;
    for (const auto& [name, loaded] : m_plugins)
      if (loaded->ready && loaded->metadata.type == PluginType::OutputFormat) {
        PluginHandle<IOutputFormatPlugin> handle(loaded);
        if (handle)
          result.push_back(std::move(handle));
      }
    return result;
  }

  auto PluginManager::listLoadedPlugins() const -> Vec<PluginMetadata> {
    const std::shared_lock lock(m_mutex);
    Vec<PluginMetadata>    result;
    for (const auto& [name, plugin] : m_plugins) result.push_back(plugin->metadata);
    return result;
  }

  auto PluginManager::listDiscoveredPlugins() const -> Vec<String> {
    (void)DracInitStaticPlugins();
    const std::shared_lock lock(m_mutex);
    Vec<String>            result;
    for (const auto& [name, entry] : GetStaticPluginRegistry()) result.push_back(name);
    for (const auto& [name, path] : m_discoveredPlugins)
      if (std::ranges::find(result, name) == result.end())
        result.push_back(name);
    std::ranges::sort(result);
    return result;
  }

  auto PluginManager::isPluginLoaded(const String& name) const -> bool {
    const std::shared_lock lock(m_mutex);
    return m_plugins.contains(name);
  }

  auto PluginManager::getPluginMetadata(const String& name) -> Result<PluginMetadata> {
    {
      const std::shared_lock lock(m_mutex);
      if (const auto iter = m_plugins.find(name); iter != m_plugins.end())
        return iter->second->metadata;
    }
    // Constructors and destructors may reenter the manager. Copy all strings
    // before releasing the temporary instance and its dynamic library.
    const auto plugin = constructPlugin(name);
    if (!plugin)
      return std::unexpected(plugin.error());
    return (*plugin)->metadata;
  }

  auto PluginManager::loadDynamicLibrary(const fs::path& path) -> Result<DynamicLibraryHandle> {
  #ifdef _WIN32
    HMODULE handle = LoadLibraryA(path.string().c_str());
    if (!handle)
      ERR_FMT(InternalError, "Failed to load DLL '{}': Error Code {}", path.string(), GetLastError());
  #else
      // macOS frameworks can retain callback blocks after a provider's timeout.
      // Keep plugin code mapped for those late invoke/copy/dispose callbacks.
    #ifdef __APPLE__
    void* handle = dlopen(path.string().c_str(), RTLD_LAZY | RTLD_NODELETE);
    #else
    void* handle = dlopen(path.string().c_str(), RTLD_LAZY);
    #endif
    if (!handle)
      ERR_FMT(InternalError, "Failed to load shared library '{}': {}", path.string(), dlerror());
  #endif

    return handle;
  }

  auto PluginManager::unloadDynamicLibrary(DynamicLibraryHandle handle) -> Unit {
    if (handle)
  #ifdef _WIN32
      FreeLibrary(handle);
  #else
      dlclose(handle);
  #endif
  }

  auto PluginManager::getCreatePluginFunc(DynamicLibraryHandle handle) -> Result<IPlugin* (*)()> {
  #ifdef _WIN32
    FARPROC func = GetProcAddress(handle, "CreatePlugin");
  #else
    void* func = dlsym(handle, "CreatePlugin");
  #endif
    if (!func)
      ERR(InternalError, "Failed to find 'CreatePlugin' function in plugin.");

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<IPlugin* (*)()>(func);
  }

  auto PluginManager::getDestroyPluginFunc(DynamicLibraryHandle handle) -> Result<void (*)(IPlugin*)> {
  #ifdef _WIN32
    FARPROC func = GetProcAddress(handle, "DestroyPlugin");
  #else
    void* func = dlsym(handle, "DestroyPlugin");
  #endif
    if (!func)
      ERR(InternalError, "Failed to find 'DestroyPlugin' function in plugin.");

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<void (*)(IPlugin*)>(func);
  }

  auto PluginManager::syncPluginLogLevel(DynamicLibraryHandle handle) -> void {
    using utils::logging::GetLogLevelPtr;
    using utils::logging::LogLevel;

    using SetLogLevelFunc = void (*)(LogLevel*);

  #ifdef _WIN32
    FARPROC func = GetProcAddress(handle, "SetPluginLogLevel");
  #else
    void* func = dlsym(handle, "SetPluginLogLevel");
  #endif
    if (func) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      auto setLogLevel = reinterpret_cast<SetLogLevelFunc>(func);
      setLogLevel(GetLogLevelPtr());
      debug_log("Synchronized log level with plugin");
    } else {
      debug_log("SetPluginLogLevel function not found in plugin (may be an older plugin)");
    }
  }

  auto InitializePlugin(const std::shared_ptr<LoadedPlugin>& plugin) -> Result<Unit> {
    const std::unique_lock lock(plugin->mutex);
    if (plugin->initialized)
      return {};
    const auto context = GetPluginContext();
    for (const auto& directory : { context.configDir, context.cacheDir, context.dataDir }) {
      std::error_code error;
      fs::create_directories(directory, error);
      if (error)
        ERR_FMT(IoError, "Cannot create plugin directory '{}': {}", directory.string(), error.message());
    }
    const auto result = plugin->instance->initialize(context, *plugin->cache);
    if (!result)
      return result;
    plugin->ready = plugin->instance->isReady();
    if (!plugin->ready)
      ERR(ApiUnavailable, "Plugin initialized but is not ready");
    plugin->initialized = true;
    return {};
  }
} // namespace draconis::core::plugin

#endif // DRAC_ENABLE_PLUGINS
