#include "../include/draconis_c.h"

#include <cstring>
#include <mutex>

#include <Drac++/Core/System.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/Plugin.hpp>
  #include <Drac++/Core/PluginConfig.hpp>
  #include <Drac++/Core/PluginManager.hpp>
  #if DRAC_PRECOMPILED_CONFIG
    #include <Drac++/Core/StaticPlugins.hpp>
  #endif
#endif

#include "Drac++/Utils/DataTypes.hpp"
#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

using namespace draconis::core::system;
using namespace draconis::utils::cache;
using namespace draconis::utils::types;

#if DRAC_ENABLE_PLUGINS
using namespace draconis::core::plugin;
#endif

// Convert C++ DracErrorCode to C DracErrorCode enum value
#define TO_C_ERROR(err) static_cast<::DracErrorCode>(static_cast<u8>((err).code))

namespace {
  auto DupString(const String& str) -> CStr* {
    CStr* result = new CStr[str.size() + 1];
    std::memcpy(result, str.c_str(), str.size() + 1);
    return result;
  }

  auto DupOptionalString(const Option<String>& opt) -> CStr* {
    if (opt.has_value())
      return DupString(*opt);

    return nullptr;
  }
} // namespace

struct DracCacheManager {
  CacheManager inner;
};

extern "C" {
  auto DracCreateCacheManager(void) -> DracCacheManager* {
    return new DracCacheManager();
  }

  auto DracDestroyCacheManager(DracCacheManager* mgr) -> void {
    delete mgr;
  }

  auto DracFreeString(PCStr str) -> void {
    delete[] str;
  }

  auto DracFreeOSInfo(DracOSInfo* info) -> void {
    if (!info)
      return;

    delete[] info->name;
    delete[] info->version;
    delete[] info->id;
    info->name    = nullptr;
    info->version = nullptr;
    info->id      = nullptr;
  }

  auto DracFreeDiskInfo(DracDiskInfo* info) -> void {
    if (!info)
      return;

    delete[] info->name;
    delete[] info->mountPoint;
    delete[] info->filesystem;
    delete[] info->driveType;
    info->name       = nullptr;
    info->mountPoint = nullptr;
    info->filesystem = nullptr;
    info->driveType  = nullptr;
  }

  auto DracFreeDiskInfoList(DracDiskInfoList* list) -> void {
    if (!list || !list->items)
      return;

    Span<DracDiskInfo> items(list->items, list->count);
    for (DracDiskInfo& item : items)
      DracFreeDiskInfo(&item);

    delete[] list->items;
    list->items = nullptr;
    list->count = 0;
  }

  auto DracFreeDisplayInfoList(DracDisplayInfoList* list) -> void {
    if (!list || !list->items)
      return;

    delete[] list->items;
    list->items = nullptr;
    list->count = 0;
  }

  auto DracFreeNetworkInterface(DracNetworkInterface* iface) -> void {
    if (!iface)
      return;

    delete[] iface->name;
    delete[] iface->ipv4Address;
    delete[] iface->ipv6Address;
    delete[] iface->macAddress;
    iface->name        = nullptr;
    iface->ipv4Address = nullptr;
    iface->ipv6Address = nullptr;
    iface->macAddress  = nullptr;
  }

  auto DracFreeNetworkInterfaceList(DracNetworkInterfaceList* list) -> void {
    if (!list || !list->items)
      return;

    Span<DracNetworkInterface> items(list->items, list->count);
    for (DracNetworkInterface& item : items)
      DracFreeNetworkInterface(&item);

    delete[] list->items;
    list->items = nullptr;
    list->count = 0;
  }

  auto DracGetUptime(void) -> uint64_t {
    Result<std::chrono::seconds> result = GetUptime();

    if (result.has_value())
      return static_cast<uint64_t>(result.value().count());

    return 0;
  }

  auto DracGetMemInfo(DracCacheManager* mgr, DracResourceUsage* out_usage) -> DracErrorCode {
    if (!mgr || !out_usage)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<ResourceUsage> result = GetMemInfo(mgr->inner);

    if (result.has_value()) {
      ResourceUsage& val    = result.value();
      out_usage->usedBytes  = val.usedBytes;
      out_usage->totalBytes = val.totalBytes;
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetCpuCores(DracCacheManager* mgr, DracCPUCores* out_cores) -> DracErrorCode {
    if (!mgr || !out_cores)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<CPUCores> result = GetCPUCores(mgr->inner);

    if (result.has_value()) {
      CPUCores& val       = result.value();
      out_cores->physical = val.physical;
      out_cores->logical  = val.logical;
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetOperatingSystem(DracCacheManager* mgr, DracOSInfo* out_info) -> DracErrorCode {
    if (!mgr || !out_info)
      return DRAC_ERROR_INVALID_ARGUMENT;

    *out_info = { nullptr, nullptr, nullptr };

    Result<OSInfo> result = GetOperatingSystem(mgr->inner);

    if (result.has_value()) {
      OSInfo& val       = result.value();
      out_info->name    = DupString(val.name);
      out_info->version = DupString(val.version);
      out_info->id      = DupString(val.id);
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetDesktopEnvironment(DracCacheManager* mgr, char** out_str) -> DracErrorCode {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<String> result = GetDesktopEnvironment(mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetWindowManager(DracCacheManager* mgr, char** out_str) -> DracErrorCode {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<String> result = GetWindowManager(mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetShell(DracCacheManager* mgr, char** out_str) -> DracErrorCode {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<String> result = GetShell(mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetHost(DracCacheManager* mgr, char** out_str) -> DracErrorCode {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<String> result = GetHost(mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetCPUModel(DracCacheManager* mgr, char** out_str) -> DracErrorCode {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<String> result = GetCPUModel(mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetGPUModel(DracCacheManager* mgr, char** out_str) -> DracErrorCode {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<String> result = GetGPUModel(mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetKernelVersion(DracCacheManager* mgr, char** out_str) -> DracErrorCode {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<String> result = GetKernelVersion(mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetDiskUsage(DracCacheManager* mgr, DracResourceUsage* out_usage) -> DracErrorCode {
    if (!mgr || !out_usage)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<ResourceUsage> result = GetDiskUsage(mgr->inner);

    if (result.has_value()) {
      ResourceUsage& val    = result.value();
      out_usage->usedBytes  = val.usedBytes;
      out_usage->totalBytes = val.totalBytes;
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetDisks(DracCacheManager* mgr, DracDiskInfoList* out_list) -> DracErrorCode {
    if (!mgr || !out_list)
      return DRAC_ERROR_INVALID_ARGUMENT;

    *out_list = { nullptr, 0 };

    Result<Vec<DiskInfo>> result = GetDisks(mgr->inner);

    if (result.has_value()) {
      Vec<DiskInfo>& disks = result.value();
      out_list->count      = disks.size();
      out_list->items      = new DracDiskInfo[disks.size()];

      Span<DracDiskInfo> outItems(out_list->items, out_list->count);
      usize              idx = 0;

      for (DracDiskInfo& dst : outItems) {
        DiskInfo& src     = disks[idx++];
        dst.name          = DupString(src.name);
        dst.mountPoint    = DupString(src.mountPoint);
        dst.filesystem    = DupString(src.filesystem);
        dst.driveType     = DupString(src.driveType);
        dst.totalBytes    = src.totalBytes;
        dst.usedBytes     = src.usedBytes;
        dst.isSystemDrive = src.isSystemDrive;
      }

      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetSystemDisk(DracCacheManager* mgr, DracDiskInfo* out_info) -> DracErrorCode {
    if (!mgr || !out_info)
      return DRAC_ERROR_INVALID_ARGUMENT;

    *out_info = { nullptr, nullptr, nullptr, nullptr, 0, 0, false };

    Result<DiskInfo> result = GetSystemDisk(mgr->inner);

    if (result.has_value()) {
      DiskInfo& disk          = result.value();
      out_info->name          = DupString(disk.name);
      out_info->mountPoint    = DupString(disk.mountPoint);
      out_info->filesystem    = DupString(disk.filesystem);
      out_info->driveType     = DupString(disk.driveType);
      out_info->totalBytes    = disk.totalBytes;
      out_info->usedBytes     = disk.usedBytes;
      out_info->isSystemDrive = disk.isSystemDrive;
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetOutputs(DracCacheManager* mgr, DracDisplayInfoList* out_list) -> DracErrorCode {
    if (!mgr || !out_list)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<Vec<DisplayInfo>> result = GetOutputs(mgr->inner);

    if (result.has_value()) {
      Vec<DisplayInfo>& outputs = result.value();
      out_list->count           = outputs.size();
      out_list->items           = new DracDisplayInfo[outputs.size()];

      Span<DracDisplayInfo> outItems(out_list->items, out_list->count);
      usize                 idx = 0;
      for (DracDisplayInfo& dst : outItems) {
        DisplayInfo& src = outputs[idx++];
        dst.id           = src.id;
        dst.width        = src.resolution.width;
        dst.height       = src.resolution.height;
        dst.refreshRate  = src.refreshRate;
        dst.isPrimary    = src.isPrimary;
      }
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetPrimaryOutput(DracCacheManager* mgr, DracDisplayInfo* out_info) -> DracErrorCode {
    if (!mgr || !out_info)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<DisplayInfo> result = GetPrimaryOutput(mgr->inner);

    if (result.has_value()) {
      DisplayInfo& output   = result.value();
      out_info->id          = output.id;
      out_info->width       = output.resolution.width;
      out_info->height      = output.resolution.height;
      out_info->refreshRate = output.refreshRate;
      out_info->isPrimary   = output.isPrimary;
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetNetworkInterfaces(DracCacheManager* mgr, DracNetworkInterfaceList* out_list) -> DracErrorCode {
    if (!mgr || !out_list)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<Vec<NetworkInterface>> result = GetNetworkInterfaces(mgr->inner);

    if (result.has_value()) {
      Vec<NetworkInterface>& ifaces = result.value();
      out_list->count               = ifaces.size();
      out_list->items               = new DracNetworkInterface[ifaces.size()];

      Span<DracNetworkInterface> outItems(out_list->items, out_list->count);
      usize                      idx = 0;
      for (DracNetworkInterface& dst : outItems) {
        NetworkInterface& src = ifaces[idx++];
        dst.name              = DupString(src.name);
        dst.ipv4Address       = DupOptionalString(src.ipv4Address);
        dst.ipv6Address       = DupOptionalString(src.ipv6Address);
        dst.macAddress        = DupOptionalString(src.macAddress);
        dst.isUp              = src.isUp;
        dst.isLoopback        = src.isLoopback;
      }
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetPrimaryNetworkInterface(DracCacheManager* mgr, DracNetworkInterface* out_iface) -> DracErrorCode {
    if (!mgr || !out_iface)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<NetworkInterface> result = GetPrimaryNetworkInterface(mgr->inner);

    if (result.has_value()) {
      NetworkInterface& iface = result.value();
      out_iface->name         = DupString(iface.name);
      out_iface->ipv4Address  = DupOptionalString(iface.ipv4Address);
      out_iface->ipv6Address  = DupOptionalString(iface.ipv6Address);
      out_iface->macAddress   = DupOptionalString(iface.macAddress);
      out_iface->isUp         = iface.isUp;
      out_iface->isLoopback   = iface.isLoopback;
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  auto DracGetBatteryInfo(DracCacheManager* mgr, DracBattery* out_battery) -> DracErrorCode {
    if (!mgr || !out_battery)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<Battery> result = GetBatteryInfo(mgr->inner);

    if (result.has_value()) {
      Battery& battery = result.value();

      out_battery->status = static_cast<DracBatteryStatus>(battery.status);
      out_battery->percentage =
        battery.percentage.has_value()
        ? *battery.percentage
        : UINT8_MAX;
      out_battery->timeRemainingSecs =
        battery.timeRemaining.has_value()
        ? static_cast<int64_t>(battery.timeRemaining->count())
        : -1;

      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  }

  // ============================== //
  //  Plugin System                 //
  // ============================== //

#if DRAC_ENABLE_PLUGINS
  struct DracPlugin {
    IInfoProviderPlugin* inner;
    String               name;
    bool                 ownsInstance;
  };

  static std::once_flag s_staticPluginInitFlag;
  static size_t s_staticPluginCount = 0;

  auto DracInitStaticPlugins_CAPI(void) -> size_t {
#if DRAC_PRECOMPILED_CONFIG
    std::call_once(s_staticPluginInitFlag, []() {
      s_staticPluginCount = static_cast<size_t>(::draconis::core::plugin::DracInitStaticPlugins());
    });
    return s_staticPluginCount;
#else
    return 0;
#endif
  }

  auto DracInitStaticPlugins(void) -> size_t {
    return DracInitStaticPlugins_CAPI();
  }

  auto DracInitPluginManager(void) -> void {
#if DRAC_PRECOMPILED_CONFIG
    // Static plugin mode doesn't use the dynamic PluginManager
#else
    auto& mgr = GetPluginManager();
    if (!mgr.isInitialized()) {
      PluginConfig config;
      (void)mgr.initialize(config);
    }
#endif
  }

  auto DracShutdownPluginManager(void) -> void {
#if DRAC_PRECOMPILED_CONFIG
    // Static plugin mode doesn't use the dynamic PluginManager
#else
    GetPluginManager().shutdown();
#endif
  }

  auto DracAddPluginSearchPath(const char* path) -> void {
    if (!path)
      return;
#if DRAC_PRECOMPILED_CONFIG
    // Static plugins don't use search paths
#else
    GetPluginManager().addSearchPath(std::filesystem::path(path));
#endif
  }

  auto DracDiscoverPlugins(void) -> DracPluginInfoList {
    return { nullptr, 0 };
  }

  auto DracLoadPlugin(const char* pluginId) -> DracPlugin* {
    if (!pluginId)
      return nullptr;

    (void)DracInitStaticPlugins_CAPI();

    String name(pluginId);

#if DRAC_PRECOMPILED_CONFIG
    if (!IsStaticPlugin(name))
      return nullptr;

    IPlugin* basePlugin = CreateStaticPlugin(name);
    if (!basePlugin)
      return nullptr;

    auto* infoPlugin = dynamic_cast<IInfoProviderPlugin*>(basePlugin);
    if (!infoPlugin) {
      DestroyStaticPlugin(name, basePlugin);
      return nullptr;
    }

    return new DracPlugin { infoPlugin, std::move(name), true };
#else
    auto& mgr = GetPluginManager();

    CacheManager cache;
    auto result = mgr.loadPlugin(name, cache);

    if (!result.has_value())
      return nullptr;

    auto opt = mgr.getInfoProviderByName(name);
    if (!opt.has_value())
      return nullptr;

    return new DracPlugin { *opt, std::move(name), false };
#endif
  }

  auto DracLoadPluginFromPath(const char* path) -> DracPlugin* {
    if (!path)
      return nullptr;

    std::filesystem::path pluginPath(path);
    auto parentDir = pluginPath.parent_path();
    auto stem = pluginPath.stem().string();

    auto& mgr = GetPluginManager();
    mgr.addSearchPath(parentDir);
    
    // Discover plugins in the new search path
    (void)mgr.scanForPlugins();

    CacheManager cache;
    auto result = mgr.loadPlugin(stem, cache);

    if (!result.has_value())
      return nullptr;

    auto opt = mgr.getInfoProviderByName(stem);
    if (!opt.has_value())
      return nullptr;

    return new DracPlugin { *opt, std::move(stem), false };
  }

  auto DracUnloadPlugin(DracPlugin* plugin) -> void {
    if (!plugin)
      return;

#if DRAC_PRECOMPILED_CONFIG
    if (plugin->ownsInstance && plugin->inner) {
      IPlugin* basePlugin = dynamic_cast<IPlugin*>(plugin->inner);
      if (basePlugin) {
        DestroyStaticPlugin(plugin->name, basePlugin);
      }
    }
#endif
    delete plugin;
  }

  auto DracPluginInitialize(DracPlugin* plugin, DracCacheManager* cache) -> DracErrorCode {
    if (!plugin || !plugin->inner || !cache)
      return DRAC_ERROR_INVALID_ARGUMENT;

    PluginContext ctx;
    PluginCache pluginCache(std::filesystem::temp_directory_path() / "draconis_plugins");
    Result<Unit> result = plugin->inner->initialize(ctx, pluginCache);

    if (result.has_value())
      return DRAC_SUCCESS;

    return TO_C_ERROR(result.error());
  }

  auto DracPluginIsEnabled(DracPlugin* plugin) -> bool {
    if (!plugin || !plugin->inner)
      return false;

    return plugin->inner->isEnabled();
  }

  auto DracPluginIsReady(DracPlugin* plugin) -> bool {
    if (!plugin || !plugin->inner)
      return false;

    return plugin->inner->isReady();
  }

  auto DracPluginCollectData(DracPlugin* plugin, DracCacheManager* cache) -> DracErrorCode {
    if (!plugin || !plugin->inner || !cache)
      return DRAC_ERROR_INVALID_ARGUMENT;

    PluginCache pluginCache(std::filesystem::temp_directory_path() / "draconis_plugins");
    Result<Unit> result = plugin->inner->collectData(pluginCache);

    if (result.has_value())
      return DRAC_SUCCESS;

    return TO_C_ERROR(result.error());
  }

  auto DracPluginGetJson(DracPlugin* plugin) -> char* {
    if (!plugin || !plugin->inner)
      return nullptr;

    Result<String> result = plugin->inner->toJson();
    if (!result.has_value())
      return nullptr;

    return DupString(*result);
  }

  auto DracPluginGetFields(DracPlugin* plugin) -> DracPluginFieldList {
    DracPluginFieldList result = { nullptr, 0 };

    if (!plugin || !plugin->inner)
      return result;

    Map<String, String> fields = plugin->inner->getFields();
    result.count                = fields.size();
    result.items                = new DracPluginField[fields.size()];

    size_t idx = 0;
    for (const auto& [key, value] : fields) {
      result.items[idx].key   = DupString(key);
      result.items[idx].value = DupString(value);
      ++idx;
    }

    return result;
  }

  auto DracPluginGetLastError(DracPlugin* plugin) -> char* {
    if (!plugin || !plugin->inner)
      return nullptr;

    Option<String> err = plugin->inner->getLastError();
    if (!err.has_value())
      return nullptr;

    return DupString(*err);
  }

  auto DracFreePluginFieldList(DracPluginFieldList* list) -> void {
    if (!list || !list->items)
      return;

    for (size_t i = 0; i < list->count; ++i) {
      delete[] list->items[i].key;
      delete[] list->items[i].value;
    }

    delete[] list->items;
    list->items = nullptr;
    list->count = 0;
  }

  auto DracFreePluginInfoList(DracPluginInfoList* list) -> void {
    if (!list || !list->items)
      return;

    for (size_t i = 0; i < list->count; ++i) {
      delete[] list->items[i].name;
      delete[] list->items[i].version;
      delete[] list->items[i].author;
      delete[] list->items[i].description;
    }

    delete[] list->items;
    list->items = nullptr;
    list->count = 0;
  }
#else
  // Stub implementations when plugins are disabled
  struct DracPlugin {
    int dummy;
  };

  auto DracInitStaticPlugins(void) -> size_t { return 0; }
  auto DracInitPluginManager(void) -> void {}
  auto DracShutdownPluginManager(void) -> void {}
  auto DracAddPluginSearchPath(const char*) -> void {}

  auto DracDiscoverPlugins(void) -> DracPluginInfoList {
    return { nullptr, 0 };
  }

  auto DracFreePluginInfoList(DracPluginInfoList* list) -> void {
    if (list) {
      list->items = nullptr;
      list->count = 0;
    }
  }

  auto DracLoadPlugin(const char*) -> DracPlugin* {
    return nullptr;
  }

  auto DracLoadPluginFromPath(const char*) -> DracPlugin* {
    return nullptr;
  }

  auto DracUnloadPlugin(DracPlugin*) -> void {}

  auto DracPluginInitialize(DracPlugin*, DracCacheManager*) -> DracErrorCode {
    return DRAC_ERROR_NOT_SUPPORTED;
  }

  auto DracPluginIsEnabled(DracPlugin*) -> bool {
    return false;
  }

  auto DracPluginIsReady(DracPlugin*) -> bool {
    return false;
  }

  auto DracPluginCollectData(DracPlugin*, DracCacheManager*) -> DracErrorCode {
    return DRAC_ERROR_NOT_SUPPORTED;
  }

  auto DracPluginGetJson(DracPlugin*) -> char* {
    return nullptr;
  }

  auto DracPluginGetFields(DracPlugin*) -> DracPluginFieldList {
    return { nullptr, 0 };
  }

  auto DracPluginGetLastError(DracPlugin*) -> char* {
    return nullptr;
  }

  auto DracFreePluginFieldList(DracPluginFieldList* list) -> void {
    if (list) {
      list->items = nullptr;
      list->count = 0;
    }
  }
#endif
}
