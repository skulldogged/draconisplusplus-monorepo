module c3::draconis_raw;

// Low-level bindings to the C API in c/include/draconis_c.h.
// C3 does not parse C headers, so declarations are mirrored here.

// Opaque handle for CacheManager
typedef DracCacheManager = void;

// Error codes matching draconis::utils::error::DracErrorCode.
// C3 gap-enum syntax is different, so model as an inline int + constants.
typedef DracErrorCode = inline int;
const DracErrorCode DRAC_ERROR_API_UNAVAILABLE     = 0;
const DracErrorCode DRAC_ERROR_CONFIGURATION_ERROR = 1;
const DracErrorCode DRAC_ERROR_CORRUPTED_DATA      = 2;
const DracErrorCode DRAC_ERROR_INTERNAL_ERROR      = 3;
const DracErrorCode DRAC_ERROR_INVALID_ARGUMENT    = 4;
const DracErrorCode DRAC_ERROR_IO_ERROR            = 5;
const DracErrorCode DRAC_ERROR_NETWORK_ERROR       = 6;
const DracErrorCode DRAC_ERROR_NOT_FOUND           = 7;
const DracErrorCode DRAC_ERROR_NOT_SUPPORTED       = 8;
const DracErrorCode DRAC_ERROR_OTHER               = 9;
const DracErrorCode DRAC_ERROR_OUT_OF_MEMORY       = 10;
const DracErrorCode DRAC_ERROR_PARSE_ERROR         = 11;
const DracErrorCode DRAC_ERROR_PERMISSION_DENIED   = 12;
const DracErrorCode DRAC_ERROR_PERMISSION_REQUIRED = 13;
const DracErrorCode DRAC_ERROR_PLATFORM_SPECIFIC   = 14;
const DracErrorCode DRAC_ERROR_RESOURCE_EXHAUSTED  = 15;
const DracErrorCode DRAC_ERROR_TIMEOUT             = 16;
const DracErrorCode DRAC_ERROR_UNAVAILABLE_FEATURE = 17;
const DracErrorCode DRAC_SUCCESS                   = 255;

struct DracResourceUsage {
  ulong usedBytes;
  ulong totalBytes;
}

struct DracCPUCores {
  usz physical;
  usz logical;
}

struct DracOSInfo {
  char* name;
  char* version;
  char* id;
}

struct DracDiskInfo {
  char* name;
  char* mountPoint;
  char* filesystem;
  char* driveType;
  ulong totalBytes;
  ulong usedBytes;
  bool isSystemDrive;
}

struct DracDiskInfoList {
  DracDiskInfo* items;
  usz count;
}

struct DracDisplayInfo {
  ulong id;
  ulong width;
  ulong height;
  double refreshRate;
  bool isPrimary;
}

struct DracDisplayInfoList {
  DracDisplayInfo* items;
  usz count;
}

struct DracNetworkInterface {
  char* name;
  char* ipv4Address;
  char* ipv6Address;
  char* macAddress;
  bool isUp;
  bool isLoopback;
}

struct DracNetworkInterfaceList {
  DracNetworkInterface* items;
  usz count;
}

typedef DracBatteryStatus = inline int;
const DracBatteryStatus DRAC_BATTERY_UNKNOWN     = 0;
const DracBatteryStatus DRAC_BATTERY_CHARGING    = 1;
const DracBatteryStatus DRAC_BATTERY_DISCHARGING = 2;
const DracBatteryStatus DRAC_BATTERY_FULL        = 3;
const DracBatteryStatus DRAC_BATTERY_NOT_PRESENT = 4;

struct DracBattery {
  DracBatteryStatus status;
  char percentage;
  long timeRemainingSecs;
}

// Plugin system types
typedef DracPlugin = void;

struct DracPluginInfo {
  char* name;
  char* version;
  char* author;
  char* description;
}

struct DracPluginInfoList {
  DracPluginInfo* items;
  usz count;
}

struct DracPluginField {
  char* key;
  char* value;
}

struct DracPluginFieldList {
  DracPluginField* items;
  usz count;
}

// C3 requires function names to be lower-case; use @cname to bind to the C symbols.
extern fn DracCacheManager* drac_create_cache_manager() @cname("DracCreateCacheManager");
extern fn void drac_destroy_cache_manager(DracCacheManager* mgr) @cname("DracDestroyCacheManager");
extern fn void drac_free_string(char* str) @cname("DracFreeString");
extern fn void drac_free_os_info(DracOSInfo* info) @cname("DracFreeOSInfo");
extern fn void drac_free_disk_info(DracDiskInfo* info) @cname("DracFreeDiskInfo");
extern fn void drac_free_disk_info_list(DracDiskInfoList* list) @cname("DracFreeDiskInfoList");
extern fn void drac_free_display_info_list(DracDisplayInfoList* list) @cname("DracFreeDisplayInfoList");
extern fn void drac_free_network_interface(DracNetworkInterface* iface) @cname("DracFreeNetworkInterface");
extern fn void drac_free_network_interface_list(DracNetworkInterfaceList* list) @cname("DracFreeNetworkInterfaceList");

extern fn ulong drac_get_uptime() @cname("DracGetUptime");
extern fn DracErrorCode drac_get_mem_info(DracCacheManager* mgr, DracResourceUsage* out_usage) @cname("DracGetMemInfo");
extern fn DracErrorCode drac_get_cpu_cores(DracCacheManager* mgr, DracCPUCores* out_cores) @cname("DracGetCpuCores");
extern fn DracErrorCode drac_get_operating_system(DracCacheManager* mgr, DracOSInfo* out_info) @cname("DracGetOperatingSystem");
extern fn DracErrorCode drac_get_desktop_environment(DracCacheManager* mgr, char** out_str) @cname("DracGetDesktopEnvironment");
extern fn DracErrorCode drac_get_window_manager(DracCacheManager* mgr, char** out_str) @cname("DracGetWindowManager");
extern fn DracErrorCode drac_get_shell(DracCacheManager* mgr, char** out_str) @cname("DracGetShell");
extern fn DracErrorCode drac_get_host(DracCacheManager* mgr, char** out_str) @cname("DracGetHost");
extern fn DracErrorCode drac_get_cpu_model(DracCacheManager* mgr, char** out_str) @cname("DracGetCPUModel");
extern fn DracErrorCode drac_get_gpu_model(DracCacheManager* mgr, char** out_str) @cname("DracGetGPUModel");
extern fn DracErrorCode drac_get_kernel_version(DracCacheManager* mgr, char** out_str) @cname("DracGetKernelVersion");
extern fn DracErrorCode drac_get_disk_usage(DracCacheManager* mgr, DracResourceUsage* out_usage) @cname("DracGetDiskUsage");
extern fn DracErrorCode drac_get_disks(DracCacheManager* mgr, DracDiskInfoList* out_list) @cname("DracGetDisks");
extern fn DracErrorCode drac_get_system_disk(DracCacheManager* mgr, DracDiskInfo* out_info) @cname("DracGetSystemDisk");
extern fn DracErrorCode drac_get_outputs(DracCacheManager* mgr, DracDisplayInfoList* out_list) @cname("DracGetOutputs");
extern fn DracErrorCode drac_get_primary_output(DracCacheManager* mgr, DracDisplayInfo* out_info) @cname("DracGetPrimaryOutput");
extern fn DracErrorCode drac_get_network_interfaces(DracCacheManager* mgr, DracNetworkInterfaceList* out_list) @cname("DracGetNetworkInterfaces");
extern fn DracErrorCode drac_get_primary_network_interface(DracCacheManager* mgr, DracNetworkInterface* out_iface) @cname("DracGetPrimaryNetworkInterface");
extern fn DracErrorCode drac_get_battery_info(DracCacheManager* mgr, DracBattery* out_battery) @cname("DracGetBatteryInfo");

// Plugin system
extern fn usz drac_init_static_plugins() @cname("DracInitStaticPlugins");
extern fn void drac_init_plugin_manager() @cname("DracInitPluginManager");
extern fn void drac_shutdown_plugin_manager() @cname("DracShutdownPluginManager");
extern fn void drac_add_plugin_search_path(char* path) @cname("DracAddPluginSearchPath");
extern fn DracPluginInfoList drac_discover_plugins() @cname("DracDiscoverPlugins");
extern fn DracPlugin* drac_load_plugin(char* plugin_name) @cname("DracLoadPlugin");
extern fn DracPlugin* drac_load_plugin_from_path(char* path) @cname("DracLoadPluginFromPath");
extern fn void drac_unload_plugin(DracPlugin* plugin) @cname("DracUnloadPlugin");
extern fn DracErrorCode drac_plugin_initialize(DracPlugin* plugin, DracCacheManager* cache) @cname("DracPluginInitialize");
extern fn bool drac_plugin_is_enabled(DracPlugin* plugin) @cname("DracPluginIsEnabled");
extern fn bool drac_plugin_is_ready(DracPlugin* plugin) @cname("DracPluginIsReady");
extern fn DracErrorCode drac_plugin_collect_data(DracPlugin* plugin, DracCacheManager* cache) @cname("DracPluginCollectData");
extern fn char* drac_plugin_get_json(DracPlugin* plugin) @cname("DracPluginGetJson");
extern fn DracPluginFieldList drac_plugin_get_fields(DracPlugin* plugin) @cname("DracPluginGetFields");
extern fn char* drac_plugin_get_last_error(DracPlugin* plugin) @cname("DracPluginGetLastError");
extern fn void drac_free_plugin_info_list(DracPluginInfoList* list) @cname("DracFreePluginInfoList");
extern fn void drac_free_plugin_field_list(DracPluginFieldList* list) @cname("DracFreePluginFieldList");
