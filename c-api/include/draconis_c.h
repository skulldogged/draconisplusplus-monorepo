#ifndef DRACONIS_C_H
#define DRACONIS_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(DRACONIS_C_SHARED)
    #if defined(DRACONIS_C_BUILD)
      #define DRAC_C_API __declspec(dllexport)
    #else
      #define DRAC_C_API __declspec(dllimport)
    #endif
  #else
    #define DRAC_C_API
  #endif
#else
  #if defined(DRACONIS_C_SHARED)
    #define DRAC_C_API __attribute__((visibility("default")))
  #else
    #define DRAC_C_API
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
  // Opaque handle for CacheManager
  typedef struct DracCacheManager DracCacheManager;

  // Error codes matching draconis::utils::error::DracErrorCode
  typedef enum DracErrorCode {
    DRAC_ERROR_API_UNAVAILABLE     = 0,
    DRAC_ERROR_CONFIGURATION_ERROR = 1,
    DRAC_ERROR_CORRUPTED_DATA      = 2,
    DRAC_ERROR_INTERNAL_ERROR      = 3,
    DRAC_ERROR_INVALID_ARGUMENT    = 4,
    DRAC_ERROR_IO_ERROR            = 5,
    DRAC_ERROR_NETWORK_ERROR       = 6,
    DRAC_ERROR_NOT_FOUND           = 7,
    DRAC_ERROR_NOT_SUPPORTED       = 8,
    DRAC_ERROR_OTHER               = 9,
    DRAC_ERROR_OUT_OF_MEMORY       = 10,
    DRAC_ERROR_PARSE_ERROR         = 11,
    DRAC_ERROR_PERMISSION_DENIED   = 12,
    DRAC_ERROR_PERMISSION_REQUIRED = 13,
    DRAC_ERROR_PLATFORM_SPECIFIC   = 14,
    DRAC_ERROR_RESOURCE_EXHAUSTED  = 15,
    DRAC_ERROR_TIMEOUT             = 16,
    DRAC_ERROR_UNAVAILABLE_FEATURE = 17,
    DRAC_SUCCESS                   = 255 // Not an error - operation succeeded
  } DracErrorCode;

  typedef struct DracResourceUsage {
    uint64_t usedBytes;
    uint64_t totalBytes;
  } DracResourceUsage;

  typedef struct DracCPUCores {
    size_t physical;
    size_t logical;
  } DracCPUCores;

  typedef struct DracOSInfo {
    char* name;
    char* version;
    char* id;
  } DracOSInfo;

  typedef struct DracDiskInfo {
    char*    name;
    char*    mountPoint;
    char*    filesystem;
    char*    driveType;
    uint64_t totalBytes;
    uint64_t usedBytes;
    bool     isSystemDrive;
  } DracDiskInfo;

  typedef struct DracDiskInfoList {
    DracDiskInfo* items;
    size_t        count;
  } DracDiskInfoList;

  typedef struct DracDisplayInfo {
    uint64_t id;
    uint64_t width;
    uint64_t height;
    double   refreshRate;
    bool     isPrimary;
  } DracDisplayInfo;

  typedef struct DracDisplayInfoList {
    DracDisplayInfo* items;
    size_t           count;
  } DracDisplayInfoList;

  typedef struct DracNetworkInterface {
    char* name;
    char* ipv4Address; // NULL if not available
    char* ipv6Address; // NULL if not available
    char* macAddress;  // NULL if not available
    bool  isUp;
    bool  isLoopback;
  } DracNetworkInterface;

  typedef struct DracNetworkInterfaceList {
    DracNetworkInterface* items;
    size_t                count;
  } DracNetworkInterfaceList;

  typedef enum DracBatteryStatus {
    DRAC_BATTERY_UNKNOWN     = 0,
    DRAC_BATTERY_CHARGING    = 1,
    DRAC_BATTERY_DISCHARGING = 2,
    DRAC_BATTERY_FULL        = 3,
    DRAC_BATTERY_NOT_PRESENT = 4,
  } DracBatteryStatus;

  typedef struct DracBattery {
    DracBatteryStatus status;
    uint8_t           percentage;        // UINT8_MAX (255) if not available
    int64_t           timeRemainingSecs; // -1 if not available
  } DracBattery;

  /**
   * Creates a new CacheManager instance.
   * Must be destroyed with DracDestroyCacheManager.
   */
  DRAC_C_API DracCacheManager* DracCreateCacheManager(void);

  /**
   * Destroys a CacheManager instance.
   */
  DRAC_C_API void DracDestroyCacheManager(DracCacheManager* mgr);

  /**
   * Frees a string allocated by the library.
   */
  DRAC_C_API void DracFreeString(const char* str);

  /**
   * Frees an OSInfo struct's string members.
   */
  DRAC_C_API void DracFreeOSInfo(DracOSInfo* info);

  /**
   * Frees a DiskInfo struct's string members.
   */
  DRAC_C_API void DracFreeDiskInfo(DracDiskInfo* info);

  /**
   * Frees a DiskInfoList and all its contents.
   */
  DRAC_C_API void DracFreeDiskInfoList(DracDiskInfoList* list);

  /**
   * Frees a DisplayInfoList.
   */
  DRAC_C_API void DracFreeDisplayInfoList(DracDisplayInfoList* list);

  /**
   * Frees a NetworkInterface struct's string members.
   */
  DRAC_C_API void DracFreeNetworkInterface(DracNetworkInterface* iface);

  /**
   * Frees a NetworkInterfaceList and all its contents.
   */
  DRAC_C_API void DracFreeNetworkInterfaceList(DracNetworkInterfaceList* list);

  /**
   * Gets the system uptime in seconds.
   * @return Uptime in seconds, 0 on error.
   */
  DRAC_C_API uint64_t DracGetUptime(void);

  /**
   * Gets memory usage information.
   * @param mgr The cache manager instance.
   * @param out_usage Pointer to struct to receive data.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetMemInfo(DracCacheManager* mgr, DracResourceUsage* out_usage);

  /**
   * Gets CPU cores information.
   * @param mgr The cache manager instance.
   * @param out_cores Pointer to struct to receive data.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetCpuCores(DracCacheManager* mgr, DracCPUCores* out_cores);

  /**
   * Gets operating system information.
   * @param mgr The cache manager instance.
   * @param out_info Pointer to struct to receive data. Caller must free with DracFreeOSInfo.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetOperatingSystem(DracCacheManager* mgr, DracOSInfo* out_info);

  /**
   * Gets the desktop environment name.
   * @param mgr The cache manager instance.
   * @param out_str Pointer to receive allocated string. Caller must free with DracFreeString.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetDesktopEnvironment(DracCacheManager* mgr, char** out_str);

  /**
   * Gets the window manager name.
   * @param mgr The cache manager instance.
   * @param out_str Pointer to receive allocated string. Caller must free with DracFreeString.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetWindowManager(DracCacheManager* mgr, char** out_str);

  /**
   * Gets the current shell name.
   * @param mgr The cache manager instance.
   * @param out_str Pointer to receive allocated string. Caller must free with DracFreeString.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetShell(DracCacheManager* mgr, char** out_str);

  /**
   * Gets the host/machine name.
   * @param mgr The cache manager instance.
   * @param out_str Pointer to receive allocated string. Caller must free with DracFreeString.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetHost(DracCacheManager* mgr, char** out_str);

  /**
   * Gets the CPU model name.
   * @param mgr The cache manager instance.
   * @param out_str Pointer to receive allocated string. Caller must free with DracFreeString.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetCPUModel(DracCacheManager* mgr, char** out_str);

  /**
   * Gets the GPU model name.
   * @param mgr The cache manager instance.
   * @param out_str Pointer to receive allocated string. Caller must free with DracFreeString.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetGPUModel(DracCacheManager* mgr, char** out_str);

  /**
   * Gets the kernel version.
   * @param mgr The cache manager instance.
   * @param out_str Pointer to receive allocated string. Caller must free with DracFreeString.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetKernelVersion(DracCacheManager* mgr, char** out_str);

  /**
   * Gets total disk usage across all disks.
   * @param mgr The cache manager instance.
   * @param out_usage Pointer to struct to receive data.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetDiskUsage(DracCacheManager* mgr, DracResourceUsage* out_usage);

  /**
   * Gets information about all disks.
   * @param mgr The cache manager instance.
   * @param out_list Pointer to struct to receive data. Caller must free with DracFreeDiskInfoList.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetDisks(DracCacheManager* mgr, DracDiskInfoList* out_list);

  /**
   * Gets information about the system disk.
   * @param mgr The cache manager instance.
   * @param out_info Pointer to struct to receive data. Caller must free with DracFreeDiskInfo.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetSystemDisk(DracCacheManager* mgr, DracDiskInfo* out_info);

  /**
   * Gets information about all display outputs.
   * @param mgr The cache manager instance.
   * @param out_list Pointer to struct to receive data. Caller must free with DracFreeDisplayInfoList.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetOutputs(DracCacheManager* mgr, DracDisplayInfoList* out_list);

  /**
   * Gets information about the primary display output.
   * @param mgr The cache manager instance.
   * @param out_info Pointer to struct to receive data.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetPrimaryOutput(DracCacheManager* mgr, DracDisplayInfo* out_info);

  /**
   * Gets information about all network interfaces.
   * @param mgr The cache manager instance.
   * @param out_list Pointer to struct to receive data. Caller must free with DracFreeNetworkInterfaceList.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetNetworkInterfaces(DracCacheManager* mgr, DracNetworkInterfaceList* out_list);

  /**
   * Gets information about the primary network interface.
   * @param mgr The cache manager instance.
   * @param out_iface Pointer to struct to receive data. Caller must free with DracFreeNetworkInterface.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetPrimaryNetworkInterface(DracCacheManager* mgr, DracNetworkInterface* out_iface);

  /**
   * Gets battery information.
   * @param mgr The cache manager instance.
   * @param out_battery Pointer to struct to receive data.
   * @return DRAC_SUCCESS on success, error code otherwise.
   */
  DRAC_C_API DracErrorCode DracGetBatteryInfo(DracCacheManager* mgr, DracBattery* out_battery);

  // ============================== //
  //  Plugin System                 //
  // ============================== //

  typedef struct DracPlugin DracPlugin;

  typedef struct DracPluginInfo {
    char* name;
    char* version;
    char* author;
    char* description;
  } DracPluginInfo;

  typedef struct DracPluginInfoList {
    DracPluginInfo* items;
    size_t          count;
  } DracPluginInfoList;

  /**
   * Initialize static plugins.
   * MUST be called before DracLoadPlugin when using static plugins.
   * This ensures plugin registration runs even when the linker
   * discards unreferenced object files from static libraries.
   * @return Number of static plugins registered.
   */
  DRAC_C_API size_t DracInitStaticPlugins(void);

  typedef struct DracPluginField {
    char* key;
    char* value;
  } DracPluginField;

  typedef struct DracPluginFieldList {
    DracPluginField* items;
    size_t           count;
  } DracPluginFieldList;

  // Plugin manager lifecycle
  DRAC_C_API void DracInitPluginManager(void);
  DRAC_C_API void DracShutdownPluginManager(void);

  // Plugin discovery
  DRAC_C_API void DracAddPluginSearchPath(const char* path);
  DRAC_C_API DracPluginInfoList DracDiscoverPlugins(void);

  // Plugin loading - by ID (searches paths) or by explicit path
  DRAC_C_API DracPlugin* DracLoadPlugin(const char* pluginId);
  DRAC_C_API DracPlugin* DracLoadPluginFromPath(const char* path);
  DRAC_C_API void DracUnloadPlugin(DracPlugin* plugin);

  // Plugin initialization
  DRAC_C_API DracErrorCode DracPluginInitialize(DracPlugin* plugin, DracCacheManager* cache);

  // Plugin configuration - pass TOML config string to plugin
  DRAC_C_API DracErrorCode DracPluginSetConfig(DracPlugin* plugin, const char* tomlConfig);

  // Plugin state
  DRAC_C_API bool DracPluginIsEnabled(DracPlugin* plugin);
  DRAC_C_API bool DracPluginIsReady(DracPlugin* plugin);

  // Plugin data
  DRAC_C_API DracErrorCode DracPluginCollectData(DracPlugin* plugin, DracCacheManager* cache);
  DRAC_C_API char* DracPluginGetJson(DracPlugin* plugin);
  DRAC_C_API DracPluginFieldList DracPluginGetFields(DracPlugin* plugin);
  DRAC_C_API char* DracPluginGetLastError(DracPlugin* plugin);

  // Memory cleanup
  DRAC_C_API void DracFreePluginInfoList(DracPluginInfoList* list);
  DRAC_C_API void DracFreePluginFieldList(DracPluginFieldList* list);

#ifdef __cplusplus
}
#endif

#endif // DRACONIS_C_H
