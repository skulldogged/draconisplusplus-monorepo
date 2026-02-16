#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <draconis_c.h>

#include <cstdint>
#include <limits>

namespace nb = nanobind;

namespace {
  void check_error(DracErrorCode code, const char* context) {
    if (code == DRAC_SUCCESS)
      return;
    throw std::runtime_error(std::string(context) + " failed with error code " + std::to_string(static_cast<int>(code)));
  }

  std::string take_string(char* ptr) {
    if (!ptr)
      return "";
    std::string result = ptr;
    DracFreeString(ptr);
    return result;
  }
}

NB_MODULE(draconis, module) {
  module.doc() = "Python bindings for draconis++ system information library";

  nb::enum_<DracErrorCode>(module, "ErrorCode")
    .value("ApiUnavailable", DRAC_ERROR_API_UNAVAILABLE)
    .value("ConfigurationError", DRAC_ERROR_CONFIGURATION_ERROR)
    .value("CorruptedData", DRAC_ERROR_CORRUPTED_DATA)
    .value("InternalError", DRAC_ERROR_INTERNAL_ERROR)
    .value("InvalidArgument", DRAC_ERROR_INVALID_ARGUMENT)
    .value("IoError", DRAC_ERROR_IO_ERROR)
    .value("NetworkError", DRAC_ERROR_NETWORK_ERROR)
    .value("NotFound", DRAC_ERROR_NOT_FOUND)
    .value("NotSupported", DRAC_ERROR_NOT_SUPPORTED)
    .value("Other", DRAC_ERROR_OTHER)
    .value("OutOfMemory", DRAC_ERROR_OUT_OF_MEMORY)
    .value("ParseError", DRAC_ERROR_PARSE_ERROR)
    .value("PermissionDenied", DRAC_ERROR_PERMISSION_DENIED)
    .value("PermissionRequired", DRAC_ERROR_PERMISSION_REQUIRED)
    .value("PlatformSpecific", DRAC_ERROR_PLATFORM_SPECIFIC)
    .value("ResourceExhausted", DRAC_ERROR_RESOURCE_EXHAUSTED)
    .value("Timeout", DRAC_ERROR_TIMEOUT)
    .value("UnavailableFeature", DRAC_ERROR_UNAVAILABLE_FEATURE)
    .value("Success", DRAC_SUCCESS);

  nb::enum_<DracBatteryStatus>(module, "BatteryStatus")
    .value("Unknown", DRAC_BATTERY_UNKNOWN)
    .value("Charging", DRAC_BATTERY_CHARGING)
    .value("Discharging", DRAC_BATTERY_DISCHARGING)
    .value("Full", DRAC_BATTERY_FULL)
    .value("NotPresent", DRAC_BATTERY_NOT_PRESENT);

  nb::class_<DracResourceUsage>(module, "ResourceUsage")
    .def_ro("used_bytes", &DracResourceUsage::usedBytes)
    .def_ro("total_bytes", &DracResourceUsage::totalBytes)
    .def("__repr__", [](const DracResourceUsage& self) {
      return "ResourceUsage(used_bytes=" + std::to_string(self.usedBytes) +
        ", total_bytes=" + std::to_string(self.totalBytes) + ")";
    });

  nb::class_<DracCPUCores>(module, "CPUCores")
    .def_ro("physical", &DracCPUCores::physical)
    .def_ro("logical", &DracCPUCores::logical)
    .def("__repr__", [](const DracCPUCores& self) {
      return "CPUCores(physical=" + std::to_string(self.physical) +
        ", logical=" + std::to_string(self.logical) + ")";
    });

  nb::class_<DracOSInfo>(module, "OSInfo")
    .def_prop_ro("name", [](const DracOSInfo& self) { return self.name ? self.name : ""; })
    .def_prop_ro("version", [](const DracOSInfo& self) { return self.version ? self.version : ""; })
    .def_prop_ro("id", [](const DracOSInfo& self) { return self.id ? self.id : ""; })
    .def("__repr__", [](const DracOSInfo& self) {
      return "OSInfo(name='" + std::string(self.name ? self.name : "") +
        "', version='" + std::string(self.version ? self.version : "") +
        "', id='" + std::string(self.id ? self.id : "") + "')";
    });

  nb::class_<DracDiskInfo>(module, "DiskInfo")
    .def_prop_ro("name", [](const DracDiskInfo& self) { return self.name ? self.name : ""; })
    .def_prop_ro("mount_point", [](const DracDiskInfo& self) { return self.mountPoint ? self.mountPoint : ""; })
    .def_prop_ro("filesystem", [](const DracDiskInfo& self) { return self.filesystem ? self.filesystem : ""; })
    .def_prop_ro("drive_type", [](const DracDiskInfo& self) { return self.driveType ? self.driveType : ""; })
    .def_ro("total_bytes", &DracDiskInfo::totalBytes)
    .def_ro("used_bytes", &DracDiskInfo::usedBytes)
    .def_ro("is_system_drive", &DracDiskInfo::isSystemDrive)
    .def("__repr__", [](const DracDiskInfo& self) {
      return "DiskInfo(name='" + std::string(self.name ? self.name : "") +
        "', mount_point='" + std::string(self.mountPoint ? self.mountPoint : "") + "')";
    });

  nb::class_<DracDisplayInfo>(module, "DisplayInfo")
    .def_ro("id", &DracDisplayInfo::id)
    .def_ro("width", &DracDisplayInfo::width)
    .def_ro("height", &DracDisplayInfo::height)
    .def_ro("refresh_rate", &DracDisplayInfo::refreshRate)
    .def_ro("is_primary", &DracDisplayInfo::isPrimary)
    .def("__repr__", [](const DracDisplayInfo& self) {
      return "DisplayInfo(id=" + std::to_string(self.id) + ", " +
        std::to_string(self.width) + "x" + std::to_string(self.height) +
        "@" + std::to_string(self.refreshRate) + "Hz)";
    });

  nb::class_<DracNetworkInterface>(module, "NetworkInterface")
    .def_prop_ro("name", [](const DracNetworkInterface& self) { return self.name ? self.name : ""; })
    .def_prop_ro("ipv4_address", [](const DracNetworkInterface& self) -> std::optional<std::string> {
      return self.ipv4Address ? std::optional<std::string>(self.ipv4Address) : std::nullopt;
    })
    .def_prop_ro("ipv6_address", [](const DracNetworkInterface& self) -> std::optional<std::string> {
      return self.ipv6Address ? std::optional<std::string>(self.ipv6Address) : std::nullopt;
    })
    .def_prop_ro("mac_address", [](const DracNetworkInterface& self) -> std::optional<std::string> {
      return self.macAddress ? std::optional<std::string>(self.macAddress) : std::nullopt;
    })
    .def_ro("is_up", &DracNetworkInterface::isUp)
    .def_ro("is_loopback", &DracNetworkInterface::isLoopback)
    .def("__repr__", [](const DracNetworkInterface& self) {
      return "NetworkInterface(name='" + std::string(self.name ? self.name : "") + "')";
    });

  nb::class_<DracBattery>(module, "Battery")
    .def_ro("status", &DracBattery::status)
    .def_prop_ro("percentage", [](const DracBattery& self) -> std::optional<uint8_t> {
      return self.percentage == std::numeric_limits<uint8_t>::max() ? std::nullopt : std::optional<uint8_t>(self.percentage);
    })
    .def_prop_ro("time_remaining_secs", [](const DracBattery& self) -> std::optional<int64_t> {
      return self.timeRemainingSecs < 0 ? std::nullopt : std::optional<int64_t>(self.timeRemainingSecs);
    })
    .def("__repr__", [](const DracBattery& self) {
      std::string repr = "Battery(status=" + std::to_string(static_cast<int>(self.status));
      if (self.percentage != std::numeric_limits<uint8_t>::max())
        repr += ", percentage=" + std::to_string(self.percentage);
      repr += ")";
      return repr;
    });

  nb::class_<DracCacheManager>(module, "SystemInfo")
    .def(nb::init([]() {
      auto* mgr = DracCreateCacheManager();
      if (!mgr)
        throw std::runtime_error("Failed to create CacheManager");
      return mgr;
    }))
    .def("__del__", [](DracCacheManager* self) {
      if (self)
        DracDestroyCacheManager(self);
    })
    .def_static("get_uptime", []() -> uint64_t {
      return DracGetUptime();
    }, "Get system uptime in seconds")
    .def("get_mem_info", [](DracCacheManager* self) -> DracResourceUsage {
      DracResourceUsage usage {};
      check_error(DracGetMemInfo(self, &usage), "get_mem_info");
      return usage;
    }, "Get memory usage information")
    .def("get_cpu_cores", [](DracCacheManager* self) -> DracCPUCores {
      DracCPUCores cores {};
      check_error(DracGetCpuCores(self, &cores), "get_cpu_cores");
      return cores;
    }, "Get CPU core counts")
    .def("get_os", [](DracCacheManager* self) -> DracOSInfo {
      DracOSInfo info {};
      check_error(DracGetOperatingSystem(self, &info), "get_os");
      return info;
    }, "Get operating system information")
    .def("get_desktop_environment", [](DracCacheManager* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetDesktopEnvironment(self, &out), "get_desktop_environment");
      return take_string(out);
    }, "Get desktop environment name")
    .def("get_window_manager", [](DracCacheManager* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetWindowManager(self, &out), "get_window_manager");
      return take_string(out);
    }, "Get window manager name")
    .def("get_shell", [](DracCacheManager* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetShell(self, &out), "get_shell");
      return take_string(out);
    }, "Get current shell name")
    .def("get_host", [](DracCacheManager* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetHost(self, &out), "get_host");
      return take_string(out);
    }, "Get hostname")
    .def("get_cpu_model", [](DracCacheManager* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetCPUModel(self, &out), "get_cpu_model");
      return take_string(out);
    }, "Get CPU model name")
    .def("get_gpu_model", [](DracCacheManager* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetGPUModel(self, &out), "get_gpu_model");
      return take_string(out);
    }, "Get GPU model name")
    .def("get_kernel_version", [](DracCacheManager* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetKernelVersion(self, &out), "get_kernel_version");
      return take_string(out);
    }, "Get kernel version")
    .def("get_disk_usage", [](DracCacheManager* self) -> DracResourceUsage {
      DracResourceUsage usage {};
      check_error(DracGetDiskUsage(self, &usage), "get_disk_usage");
      return usage;
    }, "Get total disk usage")
    .def("get_disks", [](DracCacheManager* self) -> std::vector<DracDiskInfo> {
      DracDiskInfoList list {};
      check_error(DracGetDisks(self, &list), "get_disks");
      std::vector<DracDiskInfo> result;
      result.reserve(list.count);
      for (size_t i = 0; i < list.count; ++i)
        result.push_back(list.items[i]);
      DracFreeDiskInfoList(&list);
      return result;
    }, "Get information about all disks")
    .def("get_system_disk", [](DracCacheManager* self) -> DracDiskInfo {
      DracDiskInfo info {};
      check_error(DracGetSystemDisk(self, &info), "get_system_disk");
      return info;
    }, "Get system disk information")
    .def("get_outputs", [](DracCacheManager* self) -> std::vector<DracDisplayInfo> {
      DracDisplayInfoList list {};
      check_error(DracGetOutputs(self, &list), "get_outputs");
      std::vector<DracDisplayInfo> result;
      result.reserve(list.count);
      for (size_t i = 0; i < list.count; ++i)
        result.push_back(list.items[i]);
      DracFreeDisplayInfoList(&list);
      return result;
    }, "Get information about all display outputs")
    .def("get_primary_output", [](DracCacheManager* self) -> DracDisplayInfo {
      DracDisplayInfo info {};
      check_error(DracGetPrimaryOutput(self, &info), "get_primary_output");
      return info;
    }, "Get primary display information")
    .def("get_network_interfaces", [](DracCacheManager* self) -> std::vector<DracNetworkInterface> {
      DracNetworkInterfaceList list {};
      check_error(DracGetNetworkInterfaces(self, &list), "get_network_interfaces");
      std::vector<DracNetworkInterface> result;
      result.reserve(list.count);
      for (size_t i = 0; i < list.count; ++i)
        result.push_back(list.items[i]);
      DracFreeNetworkInterfaceList(&list);
      return result;
    }, "Get information about all network interfaces")
    .def("get_primary_network_interface", [](DracCacheManager* self) -> DracNetworkInterface {
      DracNetworkInterface iface {};
      check_error(DracGetPrimaryNetworkInterface(self, &iface), "get_primary_network_interface");
      return iface;
    }, "Get primary network interface information")
    .def("get_battery_info", [](DracCacheManager* self) -> DracBattery {
      DracBattery bat {};
      check_error(DracGetBatteryInfo(self, &bat), "get_battery_info");
      return bat;
    }, "Get battery information");

  nb::class_<DracPlugin>(module, "Plugin")
    .def_static("load", [](const std::string& name) -> DracPlugin* {
      auto* plugin = DracLoadPlugin(name.c_str());
      if (!plugin)
        throw std::runtime_error("Failed to load plugin: " + name);
      return plugin;
    }, nb::arg("name"), "Load a plugin by name")
    .def_static("load_from_path", [](const std::string& path) -> DracPlugin* {
      auto* plugin = DracLoadPluginFromPath(path.c_str());
      if (!plugin)
        throw std::runtime_error("Failed to load plugin from path: " + path);
      return plugin;
    }, nb::arg("path"), "Load a plugin from a specific path")
    .def("__del__", [](DracPlugin* self) {
      if (self)
        DracUnloadPlugin(self);
    })
    .def("initialize", [](DracPlugin* self, DracCacheManager* cache) {
      check_error(DracPluginInitialize(self, cache), "plugin initialize");
    }, nb::arg("cache"), "Initialize the plugin")
    .def("is_enabled", [](DracPlugin* self) -> bool {
      return DracPluginIsEnabled(self);
    }, "Check if plugin is enabled")
    .def("is_ready", [](DracPlugin* self) -> bool {
      return DracPluginIsReady(self);
    }, "Check if plugin is ready")
    .def("collect_data", [](DracPlugin* self, DracCacheManager* cache) {
      check_error(DracPluginCollectData(self, cache), "plugin collect_data");
    }, nb::arg("cache"), "Collect data from the plugin")
    .def("get_json", [](DracPlugin* self) -> std::string {
      char* json = DracPluginGetJson(self);
      return take_string(json);
    }, "Get plugin data as JSON")
    .def("get_fields", [](DracPlugin* self) -> std::vector<std::pair<std::string, std::string>> {
      DracPluginFieldList fields = DracPluginGetFields(self);
      std::vector<std::pair<std::string, std::string>> result;
      result.reserve(fields.count);
      for (size_t i = 0; i < fields.count; ++i) {
        const auto& field = fields.items[i];
        result.emplace_back(
          field.key ? field.key : "",
          field.value ? field.value : ""
        );
      }
      DracFreePluginFieldList(&fields);
      return result;
    }, "Get plugin data as key-value pairs")
    .def("get_last_error", [](DracPlugin* self) -> std::string {
      char* err = DracPluginGetLastError(self);
      return take_string(err);
    }, "Get the last error message from the plugin");

  module.def("init_static_plugins", []() -> size_t {
    return DracInitStaticPlugins();
  }, "Initialize static plugins and return count");

  module.def("init_plugin_manager", []() {
    DracInitPluginManager();
  }, "Initialize the plugin manager");

  module.def("shutdown_plugin_manager", []() {
    DracShutdownPluginManager();
  }, "Shutdown the plugin manager");

  module.def("add_plugin_search_path", [](const std::string& path) {
    DracAddPluginSearchPath(path.c_str());
  }, nb::arg("path"), "Add a search path for plugin discovery");
}
