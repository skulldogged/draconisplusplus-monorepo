#include <cstdint>
#include <draconis_c.h>
#include <limits>
#include <memory>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <utility>

namespace nb = nanobind;

namespace {
  template <typename T, void (*Free)(T*)>
  struct OwnedValue {
    T value {};
    explicit OwnedValue(T native) : value(native) {}
    OwnedValue(const OwnedValue&) = delete;
    OwnedValue(OwnedValue&& other) noexcept : value(std::exchange(other.value, {})) {}
    ~OwnedValue() {
      Free(&value);
    }
  };
  using OsInfo           = OwnedValue<DracOSInfo, DracFreeOSInfo>;
  using DiskInfo         = OwnedValue<DracDiskInfo, DracFreeDiskInfo>;
  using NetworkInterface = OwnedValue<DracNetworkInterface, DracFreeNetworkInterface>;
  struct SystemInfo {
    std::unique_ptr<DracCacheManager, decltype(&DracDestroyCacheManager)> value { DracCreateCacheManager(), &DracDestroyCacheManager };
    SystemInfo() {
      if (!value)
        throw std::runtime_error("Failed to create CacheManager");
    }
  };
  struct Plugin {
    std::unique_ptr<DracPlugin, decltype(&DracUnloadPlugin)> value;
    explicit Plugin(DracPlugin* native) : value(native, &DracUnloadPlugin) {}
  };
  void check_error(DracErrorCode code, const char* context) {
    if (code == DRAC_SUCCESS)
      return;
    throw std::runtime_error(std::string(context) + " failed with error code " + std::to_string(static_cast<int>(code)));
  }

  std::string take_string(char* ptr) {
    if (!ptr)
      return "";
    const std::unique_ptr<const char, decltype(&DracFreeString)> owner(ptr, &DracFreeString);
    return std::string(ptr);
  }

  nb::object plugin_field_value_to_python(const DracPluginFieldValue& value) {
    switch (value.type) {
      case DRAC_PLUGIN_FIELD_BOOL:
        return nb::bool_(value.boolValue);
      case DRAC_PLUGIN_FIELD_I64:
        return nb::int_(value.i64Value);
      case DRAC_PLUGIN_FIELD_U64:
        return nb::int_(value.u64Value);
      case DRAC_PLUGIN_FIELD_F64:
        return nb::float_(value.f64Value);
      case DRAC_PLUGIN_FIELD_STRING:
        return nb::str(value.stringValue ? value.stringValue : "");
      case DRAC_PLUGIN_FIELD_ARRAY: {
        nb::list result;
        for (size_t i = 0; i < value.arrayValue.count; ++i)
          result.append(plugin_field_value_to_python(value.arrayValue.items[i]));
        return std::move(result);
      }
      case DRAC_PLUGIN_FIELD_OBJECT: {
        nb::dict result;
        for (size_t i = 0; i < value.objectValue.count; ++i)
          result[nb::str(value.objectValue.items[i].key ? value.objectValue.items[i].key : "")] = plugin_field_value_to_python(value.objectValue.items[i].value);
        return std::move(result);
      }
    }

    return nb::none();
  }
} // namespace

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

  nb::class_<OsInfo>(module, "OSInfo")
    .def_prop_ro("name", [](const OsInfo& self) { return self.value.name ? self.value.name : ""; })
    .def_prop_ro("version", [](const OsInfo& self) { return self.value.version ? self.value.version : ""; })
    .def_prop_ro("id", [](const OsInfo& self) { return self.value.id ? self.value.id : ""; })
    .def("__repr__", [](const OsInfo& self) {
      return "OSInfo(name='" + std::string(self.value.name ? self.value.name : "") +
        "', version='" + std::string(self.value.version ? self.value.version : "") +
        "', id='" + std::string(self.value.id ? self.value.id : "") + "')";
    });

  nb::class_<DiskInfo>(module, "DiskInfo")
    .def_prop_ro("name", [](const DiskInfo& self) { return self.value.name ? self.value.name : ""; })
    .def_prop_ro("mount_point", [](const DiskInfo& self) { return self.value.mountPoint ? self.value.mountPoint : ""; })
    .def_prop_ro("filesystem", [](const DiskInfo& self) { return self.value.filesystem ? self.value.filesystem : ""; })
    .def_prop_ro("drive_type", [](const DiskInfo& self) { return self.value.driveType ? self.value.driveType : ""; })
    .def_prop_ro("total_bytes", [](const DiskInfo& self) { return self.value.totalBytes; })
    .def_prop_ro("used_bytes", [](const DiskInfo& self) { return self.value.usedBytes; })
    .def_prop_ro("is_system_drive", [](const DiskInfo& self) { return self.value.isSystemDrive; })
    .def("__repr__", [](const DiskInfo& self) {
      return "DiskInfo(name='" + std::string(self.value.name ? self.value.name : "") +
        "', mount_point='" + std::string(self.value.mountPoint ? self.value.mountPoint : "") + "')";
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

  nb::class_<NetworkInterface>(module, "NetworkInterface")
    .def_prop_ro("name", [](const NetworkInterface& self) { return self.value.name ? self.value.name : ""; })
    .def_prop_ro("ipv4_address", [](const NetworkInterface& self) -> std::optional<std::string> {
      return self.value.ipv4Address ? std::optional<std::string>(self.value.ipv4Address) : std::nullopt;
    })
    .def_prop_ro("ipv6_address", [](const NetworkInterface& self) -> std::optional<std::string> {
      return self.value.ipv6Address ? std::optional<std::string>(self.value.ipv6Address) : std::nullopt;
    })
    .def_prop_ro("mac_address", [](const NetworkInterface& self) -> std::optional<std::string> {
      return self.value.macAddress ? std::optional<std::string>(self.value.macAddress) : std::nullopt;
    })
    .def_prop_ro("is_up", [](const NetworkInterface& self) { return self.value.isUp; })
    .def_prop_ro("is_loopback", [](const NetworkInterface& self) { return self.value.isLoopback; })
    .def("__repr__", [](const NetworkInterface& self) {
      return "NetworkInterface(name='" + std::string(self.value.name ? self.value.name : "") + "')";
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

  nb::class_<SystemInfo>(module, "SystemInfo")
    .def(nb::init<>())
    .def("close", [](SystemInfo& self) { self.value.reset(); })
    .def_static("get_uptime", []() -> uint64_t { return DracGetUptime(); }, "Get system uptime in seconds")
    .def("get_mem_info", [](SystemInfo* self) -> DracResourceUsage {
      DracResourceUsage usage {};
      check_error(DracGetMemInfo(self->value.get(), &usage), "get_mem_info");
      return usage; }, "Get memory usage information")
    .def("get_cpu_cores", [](SystemInfo* self) -> DracCPUCores {
      DracCPUCores cores {};
      check_error(DracGetCpuCores(self->value.get(), &cores), "get_cpu_cores");
      return cores; }, "Get CPU core counts")
    .def("get_os", [](SystemInfo* self) -> OsInfo {
      DracOSInfo info {};
      check_error(DracGetOperatingSystem(self->value.get(), &info), "get_os");
      return OsInfo(info); }, "Get operating system information")
    .def("get_desktop_environment", [](SystemInfo* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetDesktopEnvironment(self->value.get(), &out), "get_desktop_environment");
      return take_string(out); }, "Get desktop environment name")
    .def("get_window_manager", [](SystemInfo* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetWindowManager(self->value.get(), &out), "get_window_manager");
      return take_string(out); }, "Get window manager name")
    .def("get_shell", [](SystemInfo* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetShell(self->value.get(), &out), "get_shell");
      return take_string(out); }, "Get current shell name")
    .def("get_host", [](SystemInfo* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetHost(self->value.get(), &out), "get_host");
      return take_string(out); }, "Get hostname")
    .def("get_cpu_model", [](SystemInfo* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetCPUModel(self->value.get(), &out), "get_cpu_model");
      return take_string(out); }, "Get CPU model name")
    .def("get_gpu_model", [](SystemInfo* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetGPUModel(self->value.get(), &out), "get_gpu_model");
      return take_string(out); }, "Get GPU model name")
    .def("get_kernel_version", [](SystemInfo* self) -> std::string {
      char* out = nullptr;
      check_error(DracGetKernelVersion(self->value.get(), &out), "get_kernel_version");
      return take_string(out); }, "Get kernel version")
    .def("get_disk_usage", [](SystemInfo* self) -> DracResourceUsage {
      DracResourceUsage usage {};
      check_error(DracGetDiskUsage(self->value.get(), &usage), "get_disk_usage");
      return usage; }, "Get total disk usage")
    .def("get_disks", [](SystemInfo* self) -> std::vector<DiskInfo> {
      DracDiskInfoList list {};
      check_error(DracGetDisks(self->value.get(), &list), "get_disks");
      OwnedValue<DracDiskInfoList, DracFreeDiskInfoList> owner(list);
      std::vector<DiskInfo> result;
      result.reserve(list.count);
      for (size_t i = 0; i < list.count; ++i)
        result.emplace_back(std::exchange(list.items[i], {}));
      return result; }, "Get information about all disks")
    .def("get_system_disk", [](SystemInfo* self) -> DiskInfo {
      DracDiskInfo info {};
      check_error(DracGetSystemDisk(self->value.get(), &info), "get_system_disk");
      return DiskInfo(info); }, "Get system disk information")
    .def("get_outputs", [](SystemInfo* self) -> std::vector<DracDisplayInfo> {
      DracDisplayInfoList list {};
      check_error(DracGetOutputs(self->value.get(), &list), "get_outputs");
      OwnedValue<DracDisplayInfoList, DracFreeDisplayInfoList> owned(list);
      std::vector<DracDisplayInfo> result;
      result.reserve(list.count);
      for (size_t i = 0; i < list.count; ++i)
        result.push_back(list.items[i]);
      return result; }, "Get information about all display outputs")
    .def("get_primary_output", [](SystemInfo* self) -> DracDisplayInfo {
      DracDisplayInfo info {};
      check_error(DracGetPrimaryOutput(self->value.get(), &info), "get_primary_output");
      return info; }, "Get primary display information")
    .def("get_network_interfaces", [](SystemInfo* self) -> std::vector<NetworkInterface> {
      DracNetworkInterfaceList list {};
      check_error(DracGetNetworkInterfaces(self->value.get(), &list), "get_network_interfaces");
      OwnedValue<DracNetworkInterfaceList, DracFreeNetworkInterfaceList> owner(list);
      std::vector<NetworkInterface> result;
      result.reserve(list.count);
      for (size_t i = 0; i < list.count; ++i)
        result.emplace_back(std::exchange(list.items[i], {}));
      return result; }, "Get information about all network interfaces")
    .def("get_primary_network_interface", [](SystemInfo* self) -> NetworkInterface {
      DracNetworkInterface iface {};
      check_error(DracGetPrimaryNetworkInterface(self->value.get(), &iface), "get_primary_network_interface");
      return NetworkInterface(iface); }, "Get primary network interface information")
    .def("get_battery_info", [](SystemInfo* self) -> DracBattery {
      DracBattery bat {};
      check_error(DracGetBatteryInfo(self->value.get(), &bat), "get_battery_info");
      return bat; }, "Get battery information");

  nb::class_<Plugin>(module, "Plugin")
    .def_static("load", [](const std::string& name) -> Plugin {
      auto* plugin = DracLoadPlugin(name.c_str());
      if (!plugin)
        throw std::runtime_error("Failed to load plugin: " + name);
      return Plugin(plugin); }, nb::arg("name"), "Load a plugin by name")
    .def_static("load_from_path", [](const std::string& path) -> Plugin {
      auto* plugin = DracLoadPluginFromPath(path.c_str());
      if (!plugin)
        throw std::runtime_error("Failed to load plugin from path: " + path);
      return Plugin(plugin); }, nb::arg("path"), "Load a plugin from a specific path")
    .def("close", [](Plugin& self) { self.value.reset(); })
    .def("initialize", [](Plugin* self, SystemInfo* cache) { check_error(DracPluginInitialize(self->value.get(), cache->value.get()), "plugin initialize"); }, nb::arg("cache"), "Initialize the plugin")
    .def("is_enabled", [](Plugin* self) -> bool { return DracPluginIsEnabled(self->value.get()); }, "Check if plugin is enabled")
    .def("is_ready", [](Plugin* self) -> bool { return DracPluginIsReady(self->value.get()); }, "Check if plugin is ready")
    .def("collect_data", [](Plugin* self, SystemInfo* cache) { check_error(DracPluginCollectData(self->value.get(), cache->value.get()), "plugin collect_data"); }, nb::arg("cache"), "Collect data from the plugin")
    .def("get_fields", [](Plugin* self) -> nb::dict {
      DracPluginFieldList fields = DracPluginGetFields(self->value.get());
      OwnedValue<DracPluginFieldList, DracFreePluginFieldList> owner(fields);
      nb::dict result;
      for (size_t i = 0; i < fields.count; ++i) {
        const auto& field = fields.items[i];
        if (field.key)
          result[nb::str(field.key)] = plugin_field_value_to_python(field.value);
      }
      return result; }, "Get plugin data as typed key-value pairs")
    .def("get_last_error", [](Plugin* self) -> std::string {
      char* err = DracPluginGetLastError(self->value.get());
      return take_string(err); }, "Get the last error message from the plugin");

  module.def("init_static_plugins", []() -> size_t { return DracInitStaticPlugins(); }, "Initialize static plugins and return count");

  module.def("init_plugin_manager", []() { DracInitPluginManager(); }, "Initialize the plugin manager");

  module.def("shutdown_plugin_manager", []() { DracShutdownPluginManager(); }, "Shutdown the plugin manager");

  module.def("add_plugin_search_path", [](const std::string& path) { DracAddPluginSearchPath(path.c_str()); }, nb::arg("path"), "Add a search path for plugin discovery");
}
