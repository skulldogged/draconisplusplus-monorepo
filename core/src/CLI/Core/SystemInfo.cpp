#include "SystemInfo.hpp"

#include <future>

#include <Drac++/Core/System.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/PluginManager.hpp>
#endif

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"

namespace draconis::core::system {
  namespace {
    using draconis::config::Config;
    using namespace draconis::utils::types;

    using enum draconis::utils::error::DracErrorCode;

#if DRAC_ENABLE_PLUGINS
    using draconis::core::plugin::IInfoProviderPlugin;
#endif

    auto GetDate() -> Result<String> {
      using std::chrono::system_clock;

      const system_clock::time_point nowTp = system_clock::now();
      const std::time_t              nowTt = system_clock::to_time_t(nowTp);

      std::tm nowTm;

#ifdef _WIN32
      if (localtime_s(&nowTm, &nowTt) == 0) {
#else
      if (localtime_r(&nowTt, &nowTm) != nullptr) {
#endif
        i32 day = nowTm.tm_mday;

        String monthBuffer(32, '\0');

        if (const usize monthLen = std::strftime(monthBuffer.data(), monthBuffer.size(), "%B", &nowTm); monthLen > 0) {
          using matchit::match, matchit::is, matchit::_, matchit::in;

          monthBuffer.resize(monthLen);

          PCStr suffix = match(day)(
            is | in(11, 13)    = "th",
            is | (_ % 10 == 1) = "st",
            is | (_ % 10 == 2) = "nd",
            is | (_ % 10 == 3) = "rd",
            is | _             = "th"
          );

          return std::format("{} {}{}", monthBuffer, day, suffix);
        }

        ERR(ParseError, "Failed to format date");
      }

      ERR(ParseError, "Failed to get local time");
    }
  } // namespace

  SystemInfo::SystemInfo(
    utils::cache::CacheManager& cache,
    const Config&               config,
    StringView                  compactTemplate
  ) {
    debug_log("SystemInfo: Starting construction");

    const bool collectAll = compactTemplate.empty();
    const auto wants      = [&](StringView key) -> bool {
      if (collectAll)
        return true;
      return compactTemplate.find(std::format("{{{}}}", key)) != StringView::npos;
    };

    // I'm not sure if AMD uses trademark symbols in their CPU models, but I know
    // Intel does. Might as well replace them with their unicode counterparts.
    auto replaceTrademarkSymbols = [](Result<String> str) -> Result<String> {
      String value = TRY(str);

      usize pos = 0;

      while ((pos = value.find("(TM)")) != String::npos)
        value.replace(pos, 4, "™");

      while ((pos = value.find("(R)")) != String::npos)
        value.replace(pos, 3, "®");

      return value;
    };

    Option<std::future<Result<String>>> windowManagerFuture;
    Option<std::future<Result<String>>> gpuModelFuture;
    if (utils::cache::CacheManager::ignoreCache.load(std::memory_order_relaxed)) {
      if (wants("wm"))
        windowManagerFuture.emplace(std::async(std::launch::async, [&cache] { return GetWindowManager(cache); }));
      if (wants("gpu"))
        gpuModelFuture.emplace(std::async(std::launch::async, [&cache] { return GetGPUModel(cache); }));
    }

    if (wants("de"))
      this->desktopEnv = GetDesktopEnvironment(cache);
    if (wants("wm") && !windowManagerFuture)
      this->windowMgr = GetWindowManager(cache);
    if (wants("os") || wants("os_name") || wants("os_version") || wants("os_id"))
      this->operatingSystem = GetOperatingSystem(cache);
    if (wants("kernel"))
      this->kernelVersion = GetKernelVersion(cache);
    if (wants("host"))
      this->host = GetHost(cache);
    if (wants("cpu"))
      this->cpuModel = replaceTrademarkSymbols(GetCPUModel(cache));
    if (wants("cpu_cores_physical") || wants("cpu_cores_logical"))
      this->cpuCores = GetCPUCores(cache);
    if (wants("gpu") && !gpuModelFuture)
      this->gpuModel = GetGPUModel(cache);
    if (wants("shell"))
      this->shell = GetShell(cache);
    if (wants("ram") || wants("memory_used_bytes") || wants("memory_total_bytes"))
      this->memInfo = GetMemInfo(cache);
    if (wants("disk") || wants("disk_used_bytes") || wants("disk_total_bytes"))
      this->diskUsage = GetDiskUsage(cache);
    if (wants("uptime") || wants("uptime_seconds"))
      this->uptime = GetUptime();
    if (wants("date"))
      this->date = GetDate();

    if (windowManagerFuture)
      this->windowMgr = windowManagerFuture->get();
    if (gpuModelFuture)
      this->gpuModel = gpuModelFuture->get();

#if DRAC_ENABLE_PACKAGECOUNT
    if (wants("packages"))
      this->packageCount = draconis::services::packages::GetTotalCount(cache, config.enabledPackageManagers);
#endif

#if DRAC_ENABLE_PLUGINS
    if (collectAll || compactTemplate.find("{plugin_") != StringView::npos)
      collectPluginData(cache);
#endif
    debug_log("SystemInfo: Construction complete");
  }

  auto SystemInfo::toMap() const -> Map<String, String> {
    Map<String, String> data;

    // Basic system info
    if (date)
      data["date"] = *date;
    if (host)
      data["host"] = *host;
    if (kernelVersion)
      data["kernel"] = *kernelVersion;
    if (shell)
      data["shell"] = *shell;

    // CPU info
    if (cpuModel)
      data["cpu"] = *cpuModel;
    if (cpuCores) {
      data["cpu_cores_physical"] = std::to_string(cpuCores->physical);
      data["cpu_cores_logical"]  = std::to_string(cpuCores->logical);
    }

    // GPU info
    if (gpuModel)
      data["gpu"] = *gpuModel;

    // Desktop environment
    if (desktopEnv)
      data["de"] = *desktopEnv;
    if (windowMgr)
      data["wm"] = *windowMgr;

    // Operating system info
    if (operatingSystem) {
      data["os"]         = std::format("{} {}", operatingSystem->name, operatingSystem->version);
      data["os_name"]    = operatingSystem->name;
      data["os_version"] = operatingSystem->version;
      if (!operatingSystem->id.empty())
        data["os_id"] = operatingSystem->id;
    }

    // Memory info
    if (memInfo) {
      data["ram"]                = std::format("{}/{}", BytesToGiB(memInfo->usedBytes), BytesToGiB(memInfo->totalBytes));
      data["memory_used_bytes"]  = std::to_string(memInfo->usedBytes);
      data["memory_total_bytes"] = std::to_string(memInfo->totalBytes);
    }

    // Disk info
    if (diskUsage) {
      data["disk"]             = std::format("{}/{}", BytesToGiB(diskUsage->usedBytes), BytesToGiB(diskUsage->totalBytes));
      data["disk_used_bytes"]  = std::to_string(diskUsage->usedBytes);
      data["disk_total_bytes"] = std::to_string(diskUsage->totalBytes);
    }

    // Uptime
    if (uptime) {
      data["uptime"]         = std::format("{}", SecondsToFormattedDuration { *uptime });
      data["uptime_seconds"] = std::to_string(uptime->count());
    }

    // Package count
#if DRAC_ENABLE_PACKAGECOUNT
    if (packageCount && *packageCount > 0)
      data["packages"] = std::to_string(*packageCount);
#endif

    // Plugin data - flatten with plugin_<pluginId>_<fieldName> format for compact templates
#if DRAC_ENABLE_PLUGINS
    for (const auto& [pluginId, fields] : pluginData)
      for (const auto& [fieldName, value] : fields)
        data[std::format("plugin_{}_{}", pluginId, fieldName)] = draconis::core::plugin::PluginFieldToString(value);
#endif

    return data;
  }

#if DRAC_ENABLE_PLUGINS
  auto SystemInfo::collectPluginData(utils::cache::CacheManager& cache) -> Unit {
    using draconis::core::plugin::GetPluginManager;

    auto& pluginManager = GetPluginManager();

    // Early exit if plugin system not initialized (zero-cost when disabled)
    if (!pluginManager.isInitialized())
      return;

    pluginManager.loadPluginsOfType(draconis::core::plugin::PluginType::InfoProvider, cache);

    // Get all info provider plugins (high-performance lookup)
    const auto infoProviderPlugins = pluginManager.getInfoProviderPlugins();

    debug_log("Found {} info provider plugins", infoProviderPlugins.size());

    if (infoProviderPlugins.empty())
      return;

    struct CollectedPlugin {
      String                               id;
      draconis::core::plugin::PluginFields fields;
      PluginDisplayInfo                    display;
      bool                                 collected = false;
    };

    const auto collectOne = [](IInfoProviderPlugin* plugin) -> Option<CollectedPlugin> {
      if (!plugin || !plugin->isReady() || !plugin->isEnabled())
        return None;

      const auto&     metadata = plugin->getMetadata();
      CollectedPlugin collected {
        .id      = plugin->getProviderId(),
        .fields  = {                               },
        .display = {
                    .icon  = plugin->getDisplayIcon(), .label = plugin->getDisplayLabel(),
                    },
      };

      try {
        PluginCache pluginCache(utils::cache::CacheManager::getPersistentCacheDir() / "plugins");
        if (auto result = plugin->collectData(pluginCache); result) {
          collected.fields    = plugin->getFields();
          collected.collected = true;
          if (auto displayValue = plugin->getDisplayValue(); displayValue)
            collected.display.value = *displayValue;
          else
            collected.display.error = displayValue.error().message;
        } else {
          collected.display.error = result.error().message;
        }
      } catch (const std::exception& exception) {
        debug_log("Exception in plugin '{}': {}", metadata.name, exception.what());
        collected.display.error = exception.what();
      } catch (...) {
        debug_log("Unknown exception in plugin '{}'", metadata.name);
        collected.display.error = "Unknown plugin exception";
      }

      if (auto lastError = plugin->getLastError())
        collected.display.error = *lastError;

      return collected;
    };

    Vec<std::future<Option<CollectedPlugin>>> futures;
    futures.reserve(infoProviderPlugins.size());
    for (IInfoProviderPlugin* plugin : infoProviderPlugins)
      futures.emplace_back(std::async(std::launch::async, collectOne, plugin));

    for (auto& future : futures)
      if (auto collected = future.get()) {
        if (collected->collected)
          pluginData.emplace(collected->id, std::move(collected->fields));
        pluginDisplay.emplace(std::move(collected->id), std::move(collected->display));
      }

    debug_log("Total plugins with data: {}", pluginData.size());
  }
#endif
} // namespace draconis::core::system
