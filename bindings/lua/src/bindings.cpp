#include <cstdint>
#include <limits>
#include <sol/sol.hpp>
#include <stdexcept>
#include <string>

#include "draconis_c.h"

namespace {
  auto error_message(DracErrorCode code) -> const char* {
    switch (code) {
      case DRAC_ERROR_API_UNAVAILABLE:     return "API unavailable";
      case DRAC_ERROR_CONFIGURATION_ERROR: return "Configuration error";
      case DRAC_ERROR_CORRUPTED_DATA:      return "Corrupted data";
      case DRAC_ERROR_INTERNAL_ERROR:      return "Internal error";
      case DRAC_ERROR_INVALID_ARGUMENT:    return "Invalid argument";
      case DRAC_ERROR_IO_ERROR:            return "I/O error";
      case DRAC_ERROR_NETWORK_ERROR:       return "Network error";
      case DRAC_ERROR_NOT_FOUND:           return "Not found";
      case DRAC_ERROR_NOT_SUPPORTED:       return "Not supported";
      case DRAC_ERROR_OTHER:               return "Other error";
      case DRAC_ERROR_OUT_OF_MEMORY:       return "Out of memory";
      case DRAC_ERROR_PARSE_ERROR:         return "Parse error";
      case DRAC_ERROR_PERMISSION_DENIED:   return "Permission denied";
      case DRAC_ERROR_PERMISSION_REQUIRED: return "Permission required";
      case DRAC_ERROR_PLATFORM_SPECIFIC:   return "Platform specific error";
      case DRAC_ERROR_RESOURCE_EXHAUSTED:  return "Resource exhausted";
      case DRAC_ERROR_TIMEOUT:             return "Timeout";
      case DRAC_ERROR_UNAVAILABLE_FEATURE: return "Unavailable feature";
      case DRAC_SUCCESS:                   return "Success";
      default:                             return "Unknown error";
    }
  }

  void check(DracErrorCode code) {
    if (code == DRAC_SUCCESS)
      return;
    throw std::runtime_error(std::string(error_message(code)) + " (" + std::to_string(static_cast<int>(code)) + ")");
  }

  auto get_string(DracCacheManager* mgr, DracErrorCode (*func)(DracCacheManager*, char**)) -> std::string {
    char*         out  = nullptr;
    DracErrorCode code = func(mgr, &out);
    check(code);
    std::string result = out ? out : "";
    if (out)
      DracFreeString(out);
    return result;
  }

  auto resource_usage_to_table(sol::state_view lua, const DracResourceUsage& usage) -> sol::table {
    sol::table table     = lua.create_table();
    table["used_bytes"]  = usage.usedBytes;
    table["total_bytes"] = usage.totalBytes;
    return table;
  }

  auto cpu_cores_to_table(sol::state_view lua, const DracCPUCores& cores) -> sol::table {
    sol::table table  = lua.create_table();
    table["physical"] = cores.physical;
    table["logical"]  = cores.logical;
    return table;
  }

  auto os_info_to_table(sol::state_view lua, DracOSInfo& info) -> sol::table {
    sol::table table = lua.create_table();
    table["name"]    = info.name ? info.name : "";
    table["version"] = info.version ? info.version : "";
    table["id"]      = info.id ? info.id : "";
    DracFreeOSInfo(&info);
    return table;
  }

  auto disk_info_to_table(sol::state_view lua, const DracDiskInfo& info) -> sol::table {
    sol::table table         = lua.create_table();
    table["name"]            = info.name ? info.name : "";
    table["mount_point"]     = info.mountPoint ? info.mountPoint : "";
    table["filesystem"]      = info.filesystem ? info.filesystem : "";
    table["drive_type"]      = info.driveType ? info.driveType : "";
    table["total_bytes"]     = info.totalBytes;
    table["used_bytes"]      = info.usedBytes;
    table["is_system_drive"] = info.isSystemDrive;
    return table;
  }

  auto display_info_to_table(sol::state_view lua, const DracDisplayInfo& info) -> sol::table {
    sol::table table      = lua.create_table();
    table["id"]           = info.id;
    table["width"]        = info.width;
    table["height"]       = info.height;
    table["refresh_rate"] = info.refreshRate;
    table["is_primary"]   = info.isPrimary;
    return table;
  }

  auto net_iface_to_table(sol::state_view lua, const DracNetworkInterface& iface) -> sol::table {
    sol::table table = lua.create_table();
    table["name"]    = iface.name ? iface.name : "";
    if (iface.ipv4Address)
      table["ipv4_address"] = iface.ipv4Address;
    if (iface.ipv6Address)
      table["ipv6_address"] = iface.ipv6Address;
    if (iface.macAddress)
      table["mac_address"] = iface.macAddress;
    table["is_up"]       = iface.isUp;
    table["is_loopback"] = iface.isLoopback;
    return table;
  }

  auto battery_to_table(sol::state_view lua, const DracBattery& bat) -> sol::table {
    sol::table table = lua.create_table();
    table["status"]  = bat.status;
    if (bat.percentage != std::numeric_limits<uint8_t>::max())
      table["percentage"] = bat.percentage;
    if (bat.timeRemainingSecs >= 0)
      table["time_remaining_secs"] = bat.timeRemainingSecs;
    return table;
  }

  struct SystemInfo {
    DracCacheManager* mgr = nullptr;

    SystemInfo() : mgr(DracCreateCacheManager()) {
      if (!mgr)
        throw std::bad_alloc();
    }

    ~SystemInfo() {
      if (mgr)
        DracDestroyCacheManager(mgr);
      mgr = nullptr;
    }

    SystemInfo(const SystemInfo&) = delete;
    SystemInfo(SystemInfo&&)      = delete;

    auto operator=(const SystemInfo&) -> SystemInfo& = delete;
    auto operator=(SystemInfo&&) -> SystemInfo&      = delete;

    [[nodiscard]] auto getMemInfo(sol::this_state state) const -> sol::table {
      sol::state_view   lua(state);
      DracResourceUsage usage {};
      check(DracGetMemInfo(mgr, &usage));
      return resource_usage_to_table(lua, usage);
    }

    [[nodiscard]] auto getCpuCores(sol::this_state state) const -> sol::table {
      sol::state_view lua(state);
      DracCPUCores    cores {};
      check(DracGetCpuCores(mgr, &cores));
      return cpu_cores_to_table(lua, cores);
    }

    [[nodiscard]] auto getOS(sol::this_state state) const -> sol::table {
      sol::state_view lua(state);
      DracOSInfo      info {};
      check(DracGetOperatingSystem(mgr, &info));
      return os_info_to_table(lua, info);
    }

    [[nodiscard]] auto getDesktopEnvironment() const -> std::string {
      return get_string(mgr, DracGetDesktopEnvironment);
    }
    [[nodiscard]] auto getWindowManager() const -> std::string {
      return get_string(mgr, DracGetWindowManager);
    }
    [[nodiscard]] auto getShell() const -> std::string {
      return get_string(mgr, DracGetShell);
    }
    [[nodiscard]] auto getHost() const -> std::string {
      return get_string(mgr, DracGetHost);
    }
    [[nodiscard]] auto getCpuModel() const -> std::string {
      return get_string(mgr, DracGetCPUModel);
    }
    [[nodiscard]] auto getGpuModel() const -> std::string {
      return get_string(mgr, DracGetGPUModel);
    }
    [[nodiscard]] auto getKernelVersion() const -> std::string {
      return get_string(mgr, DracGetKernelVersion);
    }

    [[nodiscard]] auto getDiskUsage(sol::this_state state) const -> sol::table {
      sol::state_view   lua(state);
      DracResourceUsage usage {};
      check(DracGetDiskUsage(mgr, &usage));
      return resource_usage_to_table(lua, usage);
    }

    [[nodiscard]] auto getDisks(sol::this_state state) const -> sol::table {
      sol::state_view  lua(state);
      DracDiskInfoList list {};
      check(DracGetDisks(mgr, &list));
      sol::table out = lua.create_table(static_cast<int>(list.count), 0);
      for (size_t i = 0; i < list.count; ++i)
        out[static_cast<int>(i + 1)] = disk_info_to_table(lua, *std::next(list.items, static_cast<std::ptrdiff_t>(i)));

      DracFreeDiskInfoList(&list);
      return out;
    }

    [[nodiscard]] auto getSystemDisk(sol::this_state state) const -> sol::table {
      sol::state_view lua(state);
      DracDiskInfo    info {};
      check(DracGetSystemDisk(mgr, &info));
      sol::table table = disk_info_to_table(lua, info);
      DracFreeDiskInfo(&info);
      return table;
    }

    [[nodiscard]] auto getOutputs(sol::this_state state) const -> sol::table {
      sol::state_view     lua(state);
      DracDisplayInfoList list {};
      check(DracGetOutputs(mgr, &list));
      sol::table out = lua.create_table(static_cast<int>(list.count), 0);
      for (size_t i = 0; i < list.count; ++i)
        out[static_cast<int>(i + 1)] = display_info_to_table(lua, *std::next(list.items, static_cast<std::ptrdiff_t>(i)));

      DracFreeDisplayInfoList(&list);
      return out;
    }

    [[nodiscard]] auto getPrimaryOutput(sol::this_state state) const -> sol::table {
      sol::state_view lua(state);
      DracDisplayInfo info {};
      check(DracGetPrimaryOutput(mgr, &info));
      return display_info_to_table(lua, info);
    }

    [[nodiscard]] auto getNetworkInterfaces(sol::this_state state) const -> sol::table {
      sol::state_view          lua(state);
      DracNetworkInterfaceList list {};
      check(DracGetNetworkInterfaces(mgr, &list));
      sol::table out = lua.create_table(static_cast<int>(list.count), 0);
      for (size_t i = 0; i < list.count; ++i)
        out[static_cast<int>(i + 1)] = net_iface_to_table(lua, *std::next(list.items, static_cast<std::ptrdiff_t>(i)));

      DracFreeNetworkInterfaceList(&list);
      return out;
    }

    [[nodiscard]] auto getPrimaryNetworkInterface(sol::this_state state) const -> sol::table {
      sol::state_view      lua(state);
      DracNetworkInterface iface {};
      check(DracGetPrimaryNetworkInterface(mgr, &iface));
      sol::table table = net_iface_to_table(lua, iface);
      DracFreeNetworkInterface(&iface);
      return table;
    }

    [[nodiscard]] auto getBattery(sol::this_state state) const -> sol::table {
      sol::state_view lua(state);
      DracBattery     bat {};
      check(DracGetBatteryInfo(mgr, &bat));
      return battery_to_table(lua, bat);
    }
  };

  // Plugin wrapper class
  struct Plugin {
    DracPlugin* handle = nullptr;

    Plugin() = default;

    explicit Plugin(DracPlugin* h) : handle(h) {}

    ~Plugin() {
      if (handle) {
        DracUnloadPlugin(handle);
        handle = nullptr;
      }
    }

    Plugin(const Plugin&) = delete;
    Plugin(Plugin&& other) noexcept : handle(other.handle) {
      other.handle = nullptr;
    }

    auto operator=(const Plugin&) -> Plugin& = delete;
    auto operator=(Plugin&& other) noexcept -> Plugin& {
      if (this != &other) {
        if (handle)
          DracUnloadPlugin(handle);
        handle       = other.handle;
        other.handle = nullptr;
      }
      return *this;
    }

    auto initialize(DracCacheManager* cache) -> void {
      if (!handle || !cache)
        throw std::runtime_error("Invalid plugin or cache");
      throwOnError(DracPluginInitialize(handle, cache), "Plugin initialize");
    }

    [[nodiscard]] auto isEnabled() const -> bool {
      return handle && DracPluginIsEnabled(handle);
    }

    [[nodiscard]] auto isReady() const -> bool {
      return handle && DracPluginIsReady(handle);
    }

    auto collectData(DracCacheManager* cache) -> void {
      if (!handle || !cache)
        throw std::runtime_error("Invalid plugin or cache");
      throwOnError(DracPluginCollectData(handle, cache), "Plugin collectData");
    }

    [[nodiscard]] auto getJson() const -> std::string {
      if (!handle)
        return "";
      char* json = DracPluginGetJson(handle);
      if (!json)
        return "";
      std::string result = json;
      DracFreeString(json);
      return result;
    }

    [[nodiscard]] auto getFields(sol::this_state state) const -> sol::table {
      sol::state_view lua(state);
      sol::table      result = lua.create_table();
      if (!handle)
        return result;

      DracPluginFieldList fields = DracPluginGetFields(handle);
      if (!fields.items || fields.count == 0)
        return result;

      for (size_t i = 0; i < fields.count; ++i) {
        const auto& field = fields.items[i];
        if (field.key && field.value)
          result[field.key] = field.value;
      }

      DracFreePluginFieldList(&fields);
      return result;
    }

    [[nodiscard]] auto getLastError() const -> std::string {
      if (!handle)
        return "";
      char* err = DracPluginGetLastError(handle);
      if (!err)
        return "";
      std::string result = err;
      DracFreeString(err);
      return result;
    }

  private:
    static void throwOnError(DracErrorCode code, const char* context) {
      if (code != DRAC_SUCCESS)
        throw std::runtime_error(std::string(context) + " failed with code " + std::to_string(static_cast<int>(code)));
    }
  };
} // namespace

#if defined(_WIN32) || defined(__CYGWIN__)
  #define DRAC_LUA_API extern "C" __declspec(dllexport)
#else
  #define DRAC_LUA_API extern "C"
#endif

// NOLINTNEXTLINE(readability-identifier-naming) - Lua module entry point must follow Lua convention
DRAC_LUA_API auto luaopen_draconis(lua_State* State) -> int {
  sol::state_view lua(State);
  sol::table      module = lua.create_table();

  module.new_enum<DracErrorCode>(
    "ErrorCode",
    {
      {     "ApiUnavailable",     DRAC_ERROR_API_UNAVAILABLE },
      { "ConfigurationError", DRAC_ERROR_CONFIGURATION_ERROR },
      {      "CorruptedData",      DRAC_ERROR_CORRUPTED_DATA },
      {      "InternalError",      DRAC_ERROR_INTERNAL_ERROR },
      {    "InvalidArgument",    DRAC_ERROR_INVALID_ARGUMENT },
      {            "IoError",            DRAC_ERROR_IO_ERROR },
      {       "NetworkError",       DRAC_ERROR_NETWORK_ERROR },
      {           "NotFound",           DRAC_ERROR_NOT_FOUND },
      {       "NotSupported",       DRAC_ERROR_NOT_SUPPORTED },
      {              "Other",               DRAC_ERROR_OTHER },
      {        "OutOfMemory",       DRAC_ERROR_OUT_OF_MEMORY },
      {         "ParseError",         DRAC_ERROR_PARSE_ERROR },
      {   "PermissionDenied",   DRAC_ERROR_PERMISSION_DENIED },
      { "PermissionRequired", DRAC_ERROR_PERMISSION_REQUIRED },
      {   "PlatformSpecific",   DRAC_ERROR_PLATFORM_SPECIFIC },
      {  "ResourceExhausted",  DRAC_ERROR_RESOURCE_EXHAUSTED },
      {            "Timeout",             DRAC_ERROR_TIMEOUT },
      { "UnavailableFeature", DRAC_ERROR_UNAVAILABLE_FEATURE },
      {            "Success",                   DRAC_SUCCESS },
  }
  );

  module.new_enum<DracBatteryStatus>(
    "BatteryStatus",
    {
      {     "Unknown",     DRAC_BATTERY_UNKNOWN },
      {    "Charging",    DRAC_BATTERY_CHARGING },
      { "Discharging", DRAC_BATTERY_DISCHARGING },
      {        "Full",        DRAC_BATTERY_FULL },
      {  "NotPresent", DRAC_BATTERY_NOT_PRESENT },
  }
  );

  module.set_function("get_uptime", []() -> uint64_t {
    return DracGetUptime();
  });

  // clang-format off
  module.new_usertype<SystemInfo>(
    "SystemInfo",                    sol::constructors<SystemInfo()>(),
    "get_mem_info",                  &SystemInfo::getMemInfo,
    "get_cpu_cores",                 &SystemInfo::getCpuCores,
    "get_os",                        &SystemInfo::getOS,
    "get_desktop_environment",       &SystemInfo::getDesktopEnvironment,
    "get_window_manager",            &SystemInfo::getWindowManager,
    "get_shell",                     &SystemInfo::getShell,
    "get_host",                      &SystemInfo::getHost,
    "get_cpu_model",                 &SystemInfo::getCpuModel,
    "get_gpu_model",                 &SystemInfo::getGpuModel,
    "get_kernel_version",            &SystemInfo::getKernelVersion,
    "get_disk_usage",                &SystemInfo::getDiskUsage,
    "get_disks",                     &SystemInfo::getDisks,
    "get_system_disk",               &SystemInfo::getSystemDisk,
    "get_outputs",                   &SystemInfo::getOutputs,
    "get_primary_output",            &SystemInfo::getPrimaryOutput,
    "get_network_interfaces",        &SystemInfo::getNetworkInterfaces,
    "get_primary_network_interface", &SystemInfo::getPrimaryNetworkInterface,
    "get_battery",                   &SystemInfo::getBattery
  );
  // clang-format on

  // Plugin system
  module.set_function("init_static_plugins", []() -> size_t {
    return DracInitStaticPlugins();
  });

  module.set_function("init_plugin_manager", []() {
    DracInitPluginManager();
  });

  module.set_function("shutdown_plugin_manager", []() {
    DracShutdownPluginManager();
  });

  module.set_function("add_plugin_search_path", [](const std::string& path) {
    DracAddPluginSearchPath(path.c_str());
  });

  module.set_function("load_plugin", [](const std::string& name) -> Plugin {
    DracPlugin* handle = DracLoadPlugin(name.c_str());
    if (!handle)
      throw std::runtime_error("Failed to load plugin: " + name);
    return Plugin(handle);
  });

  module.set_function("load_plugin_from_path", [](const std::string& path) -> Plugin {
    DracPlugin* handle = DracLoadPluginFromPath(path.c_str());
    if (!handle)
      throw std::runtime_error("Failed to load plugin from path: " + path);
    return Plugin(handle);
  });

  // clang-format off
  module.new_usertype<Plugin>(
    "Plugin",
    "initialize",          [](Plugin& self, SystemInfo& sys) { self.initialize(sys.mgr); },
    "is_enabled",          &Plugin::isEnabled,
    "is_ready",            &Plugin::isReady,
    "collect_data",        [](Plugin& self, SystemInfo& sys) { self.collectData(sys.mgr); },
    "get_json",            &Plugin::getJson,
    "get_fields",          &Plugin::getFields,
    "get_last_error",      &Plugin::getLastError
  );
  // clang-format on

  return module.push();
}
