#include "SystemInfo.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <ctime>
#include <exception>
#include <format>
#include <future>
#include <ranges>
#include <string>
#include <utility>

#include <Drac++/Core/Plugin.hpp>
#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Packages.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/PluginManager.hpp>
#endif

#include <Drac++/Utils/CacheManager.hpp>
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

    enum class CompactField : u32 {
      Date               = 1U << 0U,
      Host               = 1U << 1U,
      Os                 = 1U << 2U,
      OsName             = 1U << 3U,
      OsVersion          = 1U << 4U,
      OsId               = 1U << 5U,
      Kernel             = 1U << 6U,
      Cpu                = 1U << 7U,
      CpuCoresPhysical   = 1U << 8U,
      CpuCoresLogical    = 1U << 9U,
      Gpu                = 1U << 10U,
      Ram                = 1U << 11U,
      MemoryUsedBytes    = 1U << 12U,
      MemoryTotalBytes   = 1U << 13U,
      Disk               = 1U << 14U,
      DiskUsedBytes      = 1U << 15U,
      DiskTotalBytes     = 1U << 16U,
      Uptime             = 1U << 17U,
      UptimeSeconds      = 1U << 18U,
      Shell              = 1U << 19U,
      DesktopEnvironment = 1U << 20U,
      WindowManager      = 1U << 21U,
      Packages           = 1U << 22U,
    };

    struct CompactSelection {
      u32  fields {};
      bool hasPluginField {};

      [[nodiscard]] auto contains(CompactField field) const -> bool {
        return (fields & static_cast<u32>(field)) != 0;
      }
    };

    constexpr std::array COMPACT_FIELDS {
      std::pair {               StringView("date"),               CompactField::Date },
      std::pair {               StringView("host"),               CompactField::Host },
      std::pair {                 StringView("os"),                 CompactField::Os },
      std::pair {            StringView("os_name"),             CompactField::OsName },
      std::pair {         StringView("os_version"),          CompactField::OsVersion },
      std::pair {              StringView("os_id"),               CompactField::OsId },
      std::pair {             StringView("kernel"),             CompactField::Kernel },
      std::pair {                StringView("cpu"),                CompactField::Cpu },
      std::pair { StringView("cpu_cores_physical"),   CompactField::CpuCoresPhysical },
      std::pair {  StringView("cpu_cores_logical"),    CompactField::CpuCoresLogical },
      std::pair {                StringView("gpu"),                CompactField::Gpu },
      std::pair {                StringView("ram"),                CompactField::Ram },
      std::pair {  StringView("memory_used_bytes"),    CompactField::MemoryUsedBytes },
      std::pair { StringView("memory_total_bytes"),   CompactField::MemoryTotalBytes },
      std::pair {               StringView("disk"),               CompactField::Disk },
      std::pair {    StringView("disk_used_bytes"),      CompactField::DiskUsedBytes },
      std::pair {   StringView("disk_total_bytes"),     CompactField::DiskTotalBytes },
      std::pair {             StringView("uptime"),             CompactField::Uptime },
      std::pair {     StringView("uptime_seconds"),      CompactField::UptimeSeconds },
      std::pair {              StringView("shell"),              CompactField::Shell },
      std::pair {                 StringView("de"), CompactField::DesktopEnvironment },
      std::pair {                 StringView("wm"),      CompactField::WindowManager },
      std::pair {           StringView("packages"),           CompactField::Packages },
    };

    auto ParseCompactSelection(StringView compactTemplate) -> CompactSelection {
      CompactSelection selection;
      usize            position = 0;

      while ((position = compactTemplate.find('{', position)) != StringView::npos) {
        const usize end = compactTemplate.find('}', position + 1);
        if (end == StringView::npos)
          break;

        const StringView key = compactTemplate.substr(position + 1, end - position - 1);
        if (key.starts_with("plugin_"))
          selection.hasPluginField = true;

        const auto field = std::ranges::find_if(COMPACT_FIELDS, [key](const auto& candidate) -> bool {
          return candidate.first == key;
        });
        if (field != COMPACT_FIELDS.end())
          selection.fields |= static_cast<u32>(field->second);

        position = end + 1;
      }

      return selection;
    }

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

    const bool             collectAll = compactTemplate.empty();
    const CompactSelection selection  = ParseCompactSelection(compactTemplate);
    const auto             wants      = [&](CompactField field) -> bool {
      return collectAll || selection.contains(field);
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
    const bool                          wantsWindowManager = wants(CompactField::WindowManager);
    const bool                          wantsGpu           = wants(CompactField::Gpu);
    if (utils::cache::CacheManager::ignoreCache.load(std::memory_order_relaxed) && wantsWindowManager && wantsGpu) {
      windowManagerFuture.emplace(std::async(std::launch::async, [&cache] -> Result<String> { return GetWindowManager(cache); }));
      gpuModelFuture.emplace(std::async(std::launch::async, [&cache] -> Result<String> { return GetGPUModel(cache); }));
    }

    if (wants(CompactField::DesktopEnvironment))
      this->desktopEnv = GetDesktopEnvironment(cache);
    if (wantsWindowManager && !windowManagerFuture)
      this->windowMgr = GetWindowManager(cache);
    if (wants(CompactField::Os) || wants(CompactField::OsName) || wants(CompactField::OsVersion) || wants(CompactField::OsId))
      this->operatingSystem = GetOperatingSystem(cache);
    if (wants(CompactField::Kernel))
      this->kernelVersion = GetKernelVersion(cache);
    if (wants(CompactField::Host))
      this->host = GetHost(cache);
    if (wants(CompactField::Cpu))
      this->cpuModel = replaceTrademarkSymbols(GetCPUModel(cache));
    if (wants(CompactField::CpuCoresPhysical) || wants(CompactField::CpuCoresLogical))
      this->cpuCores = GetCPUCores(cache);
    if (wantsGpu && !gpuModelFuture)
      this->gpuModel = GetGPUModel(cache);
    if (wants(CompactField::Shell))
      this->shell = GetShell(cache);
    if (wants(CompactField::Ram) || wants(CompactField::MemoryUsedBytes) || wants(CompactField::MemoryTotalBytes))
      this->memInfo = GetMemInfo(cache);
    if (wants(CompactField::Disk) || wants(CompactField::DiskUsedBytes) || wants(CompactField::DiskTotalBytes))
      this->diskUsage = GetDiskUsage(cache);
    if (wants(CompactField::Uptime) || wants(CompactField::UptimeSeconds))
      this->uptime = GetUptime();
    if (wants(CompactField::Date))
      this->date = GetDate();

    if (windowManagerFuture)
      this->windowMgr = windowManagerFuture->get();
    if (gpuModelFuture)
      this->gpuModel = gpuModelFuture->get();

#if DRAC_ENABLE_PACKAGECOUNT
    if (wants(CompactField::Packages))
      this->packageCount = draconis::services::packages::GetTotalCount(cache, config.enabledPackageManagers);
#endif

#if DRAC_ENABLE_PLUGINS
    if (collectAll || selection.hasPluginField)
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

    const auto commitCollected = [this](CollectedPlugin collected) -> void {
      if (collected.collected)
        pluginData.emplace(collected.id, std::move(collected.fields));
      pluginDisplay.emplace(std::move(collected.id), std::move(collected.display));
    };

    if (infoProviderPlugins.size() == 1) {
      if (auto collected = collectOne(infoProviderPlugins.front()))
        commitCollected(std::move(*collected));
    } else {
      constexpr usize    maxPluginWorkers = 2;
      const usize        workerCount      = std::min(maxPluginWorkers, infoProviderPlugins.size());
      std::atomic<usize> nextPlugin {};

      Vec<std::future<Vec<CollectedPlugin>>> workers;
      workers.reserve(workerCount);
      for (usize worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back(std::async(std::launch::async, [&] -> Vec<CollectedPlugin> {
          Vec<CollectedPlugin> collectedPlugins;
          while (true) {
            const usize index = nextPlugin.fetch_add(1, std::memory_order_relaxed);
            if (index >= infoProviderPlugins.size())
              break;
            if (auto collected = collectOne(infoProviderPlugins.subspan(index).front()))
              collectedPlugins.emplace_back(std::move(*collected));
          }
          return collectedPlugins;
        }));
      }

      for (auto& worker : workers)
        for (auto& collected : worker.get())
          commitCollected(std::move(collected));
    }

    debug_log("Total plugins with data: {}", pluginData.size());
  }
#endif
} // namespace draconis::core::system
