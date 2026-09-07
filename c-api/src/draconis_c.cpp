#include "../include/draconis_c.h"

#include <cstring>

#include <Drac++/Core/System.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/Plugin.hpp>
  #include <Drac++/Core/PluginConfig.hpp>
  #include <Drac++/Core/PluginManager.hpp>
  #include <Drac++/Core/StaticPlugins.hpp>
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

// The C ABI intentionally represents recursive values as tagged unions and
// exposes owning arrays through pointer-plus-count structures.
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access,misc-no-recursion)

// Convert C++ DracErrorCode to C DracErrorCode enum value
#define TO_C_ERROR(err) static_cast<::DracErrorCode>(static_cast<u8>((err).code))

namespace {
  template <typename T, void (*Free)(T*)>
  class OutputGuard {
    T* m_value;

   public:
    explicit OutputGuard(T* value) : m_value(value) {}
    ~OutputGuard() {
      if (m_value)
        Free(m_value);
    }
    auto release() -> void {
      m_value = nullptr;
    }
  };
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

#if DRAC_ENABLE_PLUGINS
  auto InitStaticPluginsForCAPI() -> size_t {
    static const auto STATIC_PLUGIN_COUNT = static_cast<size_t>(::draconis::core::plugin::DracInitStaticPlugins());
    return STATIC_PLUGIN_COUNT;
  }

  auto FreePluginFieldValue(DracPluginFieldValue& value) -> void;

  auto ToCPluginFieldValue(const PluginFieldValue& value) -> DracPluginFieldValue {
    return std::visit(
      [](const auto& inner) -> DracPluginFieldValue {
        using T = std::decay_t<decltype(inner)>;

        if constexpr (std::same_as<T, bool>) {
          return {
            .type      = DRAC_PLUGIN_FIELD_BOOL,
            .boolValue = inner,
          };
        } else if constexpr (std::same_as<T, i64>) {
          return {
            .type     = DRAC_PLUGIN_FIELD_I64,
            .i64Value = inner,
          };
        } else if constexpr (std::same_as<T, u64>) {
          return {
            .type     = DRAC_PLUGIN_FIELD_U64,
            .u64Value = inner,
          };
        } else if constexpr (std::same_as<T, f64>) {
          return {
            .type     = DRAC_PLUGIN_FIELD_F64,
            .f64Value = inner,
          };
        } else if constexpr (std::same_as<T, String>) {
          return {
            .type        = DRAC_PLUGIN_FIELD_STRING,
            .stringValue = DupString(inner),
          };
        } else if constexpr (std::same_as<T, PluginFieldArray>) {
          const DracPluginFieldValueArray array {
            .items = new DracPluginFieldValue[inner.size()] {},
            .count = inner.size(),
          };

          Span<DracPluginFieldValue> items(array.items, array.count);
          usize                      index = 0;
          try {
            for (DracPluginFieldValue& item : items)
              item = ToCPluginFieldValue(inner.at(index++));
          } catch (...) {
            for (auto& item : items) FreePluginFieldValue(item);
            delete[] array.items;
            throw;
          }

          return {
            .type       = DRAC_PLUGIN_FIELD_ARRAY,
            .arrayValue = array,
          };
        } else {
          const DracPluginFieldValueObject object {
            .items = new DracPluginField[inner.size()] {},
            .count = inner.size(),
          };

          const Span<DracPluginField> items(object.items, object.count);
          usize                       index = 0;
          try {
            for (const auto& [key, fieldValue] : inner) {
              DracPluginField& item = items.subspan(index++).front();
              item.key              = DupString(key);
              item.value            = ToCPluginFieldValue(fieldValue);
            }
          } catch (...) {
            for (auto& item : items) {
              delete[] item.key;
              FreePluginFieldValue(item.value);
            }
            delete[] object.items;
            throw;
          }

          return {
            .type        = DRAC_PLUGIN_FIELD_OBJECT,
            .objectValue = object,
          };
        }
      },
      static_cast<const PluginFieldValueBase&>(value)
    );
  }

  auto FreePluginFieldValue(DracPluginFieldValue& value) -> void {
    switch (value.type) {
      case DRAC_PLUGIN_FIELD_STRING:
        delete[] value.stringValue;
        value.stringValue = nullptr;
        break;
      case DRAC_PLUGIN_FIELD_ARRAY:
        for (DracPluginFieldValue& item : Span(value.arrayValue.items, value.arrayValue.count))
          FreePluginFieldValue(item);
        delete[] value.arrayValue.items;
        value.arrayValue.items = nullptr;
        value.arrayValue.count = 0;
        break;
      case DRAC_PLUGIN_FIELD_OBJECT:
        for (DracPluginField& item : Span(value.objectValue.items, value.objectValue.count)) {
          delete[] item.key;
          item.key = nullptr;
          FreePluginFieldValue(item.value);
        }
        delete[] value.objectValue.items;
        value.objectValue.items = nullptr;
        value.objectValue.count = 0;
        break;
      case DRAC_PLUGIN_FIELD_BOOL:
      case DRAC_PLUGIN_FIELD_I64:
      case DRAC_PLUGIN_FIELD_U64:
      case DRAC_PLUGIN_FIELD_F64:
        break;
    }
  }
#endif

  // NOLINTEND(cppcoreguidelines-pro-type-union-access,misc-no-recursion)

} // namespace

struct DracCacheManager {
  std::shared_ptr<CacheManager> inner = std::make_shared<CacheManager>();
};

extern "C" {
  auto DracCreateCacheManager(void) -> DracCacheManager* try {
    return new DracCacheManager();
  } catch (...) { return {}; }

  auto DracDestroyCacheManager(DracCacheManager* mgr) -> void try {
    delete mgr;
  } catch (...) { return; }

  auto DracFreeString(PCStr str) -> void try {
    delete[] str;
  } catch (...) { return; }

  auto DracFreeOSInfo(DracOSInfo* info) -> void try {
    if (!info)
      return;

    delete[] info->name;
    delete[] info->version;
    delete[] info->id;
    info->name    = nullptr;
    info->version = nullptr;
    info->id      = nullptr;
  } catch (...) { return; }

  auto DracFreeDiskInfo(DracDiskInfo* info) -> void try {
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
  } catch (...) { return; }

  auto DracFreeDiskInfoList(DracDiskInfoList* list) -> void try {
    if (!list || !list->items)
      return;

    Span<DracDiskInfo> items(list->items, list->count);
    for (DracDiskInfo& item : items)
      DracFreeDiskInfo(&item);

    delete[] list->items;
    list->items = nullptr;
    list->count = 0;
  } catch (...) { return; }

  auto DracFreeDisplayInfoList(DracDisplayInfoList* list) -> void try {
    if (!list || !list->items)
      return;

    delete[] list->items;
    list->items = nullptr;
    list->count = 0;
  } catch (...) { return; }

  auto DracFreeNetworkInterface(DracNetworkInterface* iface) -> void try {
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
  } catch (...) { return; }

  auto DracFreeNetworkInterfaceList(DracNetworkInterfaceList* list) -> void try {
    if (!list || !list->items)
      return;

    Span<DracNetworkInterface> items(list->items, list->count);
    for (DracNetworkInterface& item : items)
      DracFreeNetworkInterface(&item);

    delete[] list->items;
    list->items = nullptr;
    list->count = 0;
  } catch (...) { return; }

  auto DracGetUptime(void) -> uint64_t try {
    Result<std::chrono::seconds> result = GetUptime();

    if (result.has_value())
      return static_cast<uint64_t>(result.value().count());

    return 0;
  } catch (...) { return {}; }

  auto DracGetMemInfo(DracCacheManager* mgr, DracResourceUsage* out_usage) -> DracErrorCode try {
    if (!mgr || !out_usage)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<ResourceUsage> result = GetMemInfo(*mgr->inner);

    if (result.has_value()) {
      const ResourceUsage& val = result.value();
      out_usage->usedBytes     = val.usedBytes;
      out_usage->totalBytes    = val.totalBytes;
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetCpuCores(DracCacheManager* mgr, DracCPUCores* out_cores) -> DracErrorCode try {
    if (!mgr || !out_cores)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<CPUCores> result = GetCPUCores(*mgr->inner);

    if (result.has_value()) {
      const CPUCores& val = result.value();
      out_cores->physical = val.physical;
      out_cores->logical  = val.logical;
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetOperatingSystem(DracCacheManager* mgr, DracOSInfo* out_info) -> DracErrorCode try {
    if (!mgr || !out_info)
      return DRAC_ERROR_INVALID_ARGUMENT;

    *out_info = {};
    OutputGuard<DracOSInfo, DracFreeOSInfo> outputGuard(out_info);

    *out_info = { .name = nullptr, .version = nullptr, .id = nullptr };

    Result<OSInfo> result = GetOperatingSystem(*mgr->inner);

    if (result.has_value()) {
      const OSInfo& val = result.value();
      out_info->name    = DupString(val.name);
      out_info->version = DupString(val.version);
      out_info->id      = DupString(val.id);
      outputGuard.release();
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetDesktopEnvironment(DracCacheManager* mgr, char** out_str) -> DracErrorCode try {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;
    *out_str = nullptr;

    Result<String> result = GetDesktopEnvironment(*mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetWindowManager(DracCacheManager* mgr, char** out_str) -> DracErrorCode try {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;
    *out_str = nullptr;

    Result<String> result = GetWindowManager(*mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetShell(DracCacheManager* mgr, char** out_str) -> DracErrorCode try {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;
    *out_str = nullptr;

    Result<String> result = GetShell(*mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetHost(DracCacheManager* mgr, char** out_str) -> DracErrorCode try {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;
    *out_str = nullptr;

    Result<String> result = GetHost(*mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetCPUModel(DracCacheManager* mgr, char** out_str) -> DracErrorCode try {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;
    *out_str = nullptr;

    Result<String> result = GetCPUModel(*mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetGPUModel(DracCacheManager* mgr, char** out_str) -> DracErrorCode try {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;
    *out_str = nullptr;

    Result<String> result = GetGPUModel(*mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetKernelVersion(DracCacheManager* mgr, char** out_str) -> DracErrorCode try {
    if (!mgr || !out_str)
      return DRAC_ERROR_INVALID_ARGUMENT;
    *out_str = nullptr;

    Result<String> result = GetKernelVersion(*mgr->inner);

    if (result.has_value()) {
      *out_str = DupString(result.value());
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetDiskUsage(DracCacheManager* mgr, DracResourceUsage* out_usage) -> DracErrorCode try {
    if (!mgr || !out_usage)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<ResourceUsage> result = GetDiskUsage(*mgr->inner);

    if (result.has_value()) {
      const ResourceUsage& val = result.value();
      out_usage->usedBytes     = val.usedBytes;
      out_usage->totalBytes    = val.totalBytes;
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetDisks(DracCacheManager* mgr, DracDiskInfoList* out_list) -> DracErrorCode try {
    if (!mgr || !out_list)
      return DRAC_ERROR_INVALID_ARGUMENT;

    *out_list = {};
    OutputGuard<DracDiskInfoList, DracFreeDiskInfoList> outputGuard(out_list);

    *out_list = { .items = nullptr, .count = 0 };

    Result<Vec<DiskInfo>> result = GetDisks(*mgr->inner);

    if (result.has_value()) {
      Vec<DiskInfo>& disks = result.value();
      out_list->count      = disks.size();
      out_list->items      = new DracDiskInfo[disks.size()] {};

      Span<DracDiskInfo> outItems(out_list->items, out_list->count);
      usize              idx = 0;

      for (DracDiskInfo& dst : outItems) {
        const DiskInfo& src = disks.at(idx++);
        dst.name            = DupString(src.name);
        dst.mountPoint      = DupString(src.mountPoint);
        dst.filesystem      = DupString(src.filesystem);
        dst.driveType       = DupString(src.driveType);
        dst.totalBytes      = src.totalBytes;
        dst.usedBytes       = src.usedBytes;
        dst.isSystemDrive   = src.isSystemDrive;
      }

      outputGuard.release();
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetSystemDisk(DracCacheManager* mgr, DracDiskInfo* out_info) -> DracErrorCode try {
    if (!mgr || !out_info)
      return DRAC_ERROR_INVALID_ARGUMENT;

    *out_info = {};
    OutputGuard<DracDiskInfo, DracFreeDiskInfo> outputGuard(out_info);

    *out_info = {
      .name          = nullptr,
      .mountPoint    = nullptr,
      .filesystem    = nullptr,
      .driveType     = nullptr,
      .totalBytes    = 0,
      .usedBytes     = 0,
      .isSystemDrive = false,
    };

    Result<DiskInfo> result = GetSystemDisk(*mgr->inner);

    if (result.has_value()) {
      const DiskInfo& disk    = result.value();
      out_info->name          = DupString(disk.name);
      out_info->mountPoint    = DupString(disk.mountPoint);
      out_info->filesystem    = DupString(disk.filesystem);
      out_info->driveType     = DupString(disk.driveType);
      out_info->totalBytes    = disk.totalBytes;
      out_info->usedBytes     = disk.usedBytes;
      out_info->isSystemDrive = disk.isSystemDrive;
      outputGuard.release();
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetOutputs(DracCacheManager* mgr, DracDisplayInfoList* out_list) -> DracErrorCode try {
    if (!mgr || !out_list)
      return DRAC_ERROR_INVALID_ARGUMENT;

    *out_list = {};
    OutputGuard<DracDisplayInfoList, DracFreeDisplayInfoList> outputGuard(out_list);

    Result<Vec<DisplayInfo>> result = GetOutputs(*mgr->inner);

    if (result.has_value()) {
      Vec<DisplayInfo>& outputs = result.value();
      out_list->count           = outputs.size();
      out_list->items           = new DracDisplayInfo[outputs.size()] {};

      Span<DracDisplayInfo> outItems(out_list->items, out_list->count);
      usize                 idx = 0;
      for (DracDisplayInfo& dst : outItems) {
        const DisplayInfo& src = outputs.at(idx++);
        dst.id                 = src.id;
        dst.width              = src.resolution.width;
        dst.height             = src.resolution.height;
        dst.refreshRate        = src.refreshRate;
        dst.isPrimary          = src.isPrimary;
      }
      outputGuard.release();
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetPrimaryOutput(DracCacheManager* mgr, DracDisplayInfo* out_info) -> DracErrorCode try {
    if (!mgr || !out_info)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<DisplayInfo> result = GetPrimaryOutput(*mgr->inner);

    if (result.has_value()) {
      const DisplayInfo& output = result.value();
      out_info->id              = output.id;
      out_info->width           = output.resolution.width;
      out_info->height          = output.resolution.height;
      out_info->refreshRate     = output.refreshRate;
      out_info->isPrimary       = output.isPrimary;
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetNetworkInterfaces(DracCacheManager* mgr, DracNetworkInterfaceList* out_list) -> DracErrorCode try {
    if (!mgr || !out_list)
      return DRAC_ERROR_INVALID_ARGUMENT;

    *out_list = {};
    OutputGuard<DracNetworkInterfaceList, DracFreeNetworkInterfaceList> outputGuard(out_list);

    Result<Vec<NetworkInterface>> result = GetNetworkInterfaces(*mgr->inner);

    if (result.has_value()) {
      Vec<NetworkInterface>& ifaces = result.value();
      out_list->count               = ifaces.size();
      out_list->items               = new DracNetworkInterface[ifaces.size()] {};

      Span<DracNetworkInterface> outItems(out_list->items, out_list->count);
      usize                      idx = 0;
      for (DracNetworkInterface& dst : outItems) {
        const NetworkInterface& src = ifaces.at(idx++);
        dst.name                    = DupString(src.name);
        dst.ipv4Address             = DupOptionalString(src.ipv4Address);
        dst.ipv6Address             = DupOptionalString(src.ipv6Address);
        dst.macAddress              = DupOptionalString(src.macAddress);
        dst.isUp                    = src.isUp;
        dst.isLoopback              = src.isLoopback;
      }
      outputGuard.release();
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetPrimaryNetworkInterface(DracCacheManager* mgr, DracNetworkInterface* out_iface) -> DracErrorCode try {
    if (!mgr || !out_iface)
      return DRAC_ERROR_INVALID_ARGUMENT;

    *out_iface = {};
    OutputGuard<DracNetworkInterface, DracFreeNetworkInterface> outputGuard(out_iface);

    Result<NetworkInterface> result = GetPrimaryNetworkInterface(*mgr->inner);

    if (result.has_value()) {
      const NetworkInterface& iface = result.value();
      out_iface->name               = DupString(iface.name);
      out_iface->ipv4Address        = DupOptionalString(iface.ipv4Address);
      out_iface->ipv6Address        = DupOptionalString(iface.ipv6Address);
      out_iface->macAddress         = DupOptionalString(iface.macAddress);
      out_iface->isUp               = iface.isUp;
      out_iface->isLoopback         = iface.isLoopback;
      outputGuard.release();
      return DRAC_SUCCESS;
    }

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracGetBatteryInfo(DracCacheManager* mgr, DracBattery* out_battery) -> DracErrorCode try {
    if (!mgr || !out_battery)
      return DRAC_ERROR_INVALID_ARGUMENT;

    Result<Battery> result = GetBatteryInfo(*mgr->inner);

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
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

#if DRAC_ENABLE_PLUGINS
  struct DracPlugin {
    PluginHandle<IInfoProviderPlugin> inner;
  };

  auto DracInitStaticPlugins(void) -> size_t try { return InitStaticPluginsForCAPI(); } catch (...) {
    return {};
  }
  auto DracInitPluginManager(void) -> void try { (void)GetPluginManager().initialize(); } catch (...) {
    return;
  }
  auto DracShutdownPluginManager(void) -> void try { GetPluginManager().shutdown(); } catch (...) {
    return;
  }
  auto DracAddPluginSearchPath(const char* path) -> void try {
    if (path)
      GetPluginManager().addSearchPath(std::filesystem::path(path));
  } catch (...) { return; }

  auto DracDiscoverPlugins(void) -> DracPluginInfoList try {
    auto& manager = GetPluginManager();
    if (!manager.initialize() || !manager.scanForPlugins())
      return {};
    const auto         names = manager.listDiscoveredPlugins();
    DracPluginInfoList result { .items = new DracPluginInfo[names.size()] {}, .count = names.size() };
    try {
      for (size_t i = 0; i < names.size(); ++i) {
        result.items[i].name = DupString(names[i]);
        // Invalid/incompatible modules remain discoverable by name. Inspect
        // valid candidates without initializing them or adding manager ownership.
        Option<PluginMetadata> metadata;
        try {
          if (auto inspected = manager.getPluginMetadata(names[i]); inspected)
            metadata = std::move(*inspected);
        } catch (const std::bad_alloc&) {
          throw;
        } catch (...) {}
        if (metadata) {
          result.items[i].version     = DupString(metadata->version);
          result.items[i].author      = DupString(metadata->author);
          result.items[i].description = DupString(metadata->description);
        }
      }
    } catch (...) {
      DracFreePluginInfoList(&result);
      throw;
    }
    return result;
  } catch (...) { return {}; }

  auto DracLoadPlugin(const char* pluginId) -> DracPlugin* try {
    if (!pluginId)
      return nullptr;
    auto& manager = GetPluginManager();
    (void)manager.initialize();
    auto result = manager.createInfoProvider(pluginId);
    if (!result)
      return nullptr;
    return new DracPlugin { std::move(*result) };
  } catch (...) { return {}; }

  auto DracLoadPluginFromPath(const char* path) -> DracPlugin* try {
    if (!path || !*path)
      return nullptr;
    const std::filesystem::path exactPath(path);
    auto                        result = GetPluginManager().createInfoProvider(exactPath.stem().string(), exactPath);
    if (!result)
      return nullptr;
    return new DracPlugin { std::move(*result) };
  } catch (...) { return {}; }

  auto DracUnloadPlugin(DracPlugin* plugin) -> void try { delete plugin; } catch (...) {
    return;
  }

  auto DracPluginInitialize(DracPlugin* plugin, DracCacheManager* cache) -> DracErrorCode try {
    if (!plugin || !plugin->inner || !cache)
      return DRAC_ERROR_INVALID_ARGUMENT;

    const auto operationLock = plugin->inner.lock();
    plugin->inner.cache().bind(cache->inner, "plugin_" + plugin->inner->getProviderId() + "_");
    Result<Unit> result = plugin->inner.initialize();

    if (result.has_value())
      return DRAC_SUCCESS;

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracPluginSetConfig(DracPlugin* plugin, const char* tomlConfig) -> DracErrorCode try {
    if (!plugin || !plugin->inner)
      return DRAC_ERROR_INVALID_ARGUMENT;

    const auto operationLock = plugin->inner.lock();
    if (!tomlConfig)
      return DRAC_SUCCESS;

    Result<Unit> result = plugin->inner->setConfig(StringView(tomlConfig));

    if (result.has_value())
      return DRAC_SUCCESS;

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracPluginIsEnabled(DracPlugin* plugin) -> bool try {
    if (!plugin || !plugin->inner)
      return false;

    const auto operationLock = plugin->inner.lock();
    return plugin->inner->isEnabled();
  } catch (...) { return {}; }

  auto DracPluginIsReady(DracPlugin* plugin) -> bool try {
    if (!plugin || !plugin->inner)
      return false;

    const auto operationLock = plugin->inner.lock();
    return plugin->inner->isReady();
  } catch (...) { return {}; }

  auto DracPluginCollectData(DracPlugin* plugin, DracCacheManager* cache) -> DracErrorCode try {
    if (!plugin || !plugin->inner || !cache)
      return DRAC_ERROR_INVALID_ARGUMENT;

    const auto operationLock = plugin->inner.lock();
    plugin->inner.cache().bind(cache->inner, "plugin_" + plugin->inner->getProviderId() + "_");
    Result<Unit> result = plugin->inner->collectData(plugin->inner.cache());

    if (result.has_value())
      return DRAC_SUCCESS;

    return TO_C_ERROR(result.error());
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracPluginGetFields(DracPlugin* plugin) -> DracPluginFieldList try {
    DracPluginFieldList result = { .items = nullptr, .count = 0 };

    if (!plugin || !plugin->inner)
      return result;

    const auto         operationLock = plugin->inner.lock();
    const PluginFields fields        = plugin->inner->getFields();
    result.count                     = fields.size();
    result.items                     = new DracPluginField[fields.size()] {};

    OutputGuard<DracPluginFieldList, DracFreePluginFieldList> outputGuard(&result);
    const Span<DracPluginField>                               items(result.items, result.count);
    size_t                                                    idx = 0;
    for (const auto& [key, value] : fields) {
      DracPluginField& item = items.subspan(idx++).front();
      item.key              = DupString(key);
      item.value            = ToCPluginFieldValue(value);
    }

    outputGuard.release();
    return result;
  } catch (...) { return {}; }

  auto DracPluginGetLastError(DracPlugin* plugin) -> char* try {
    if (!plugin || !plugin->inner)
      return nullptr;

    const auto     operationLock = plugin->inner.lock();
    Option<String> err           = plugin->inner->getLastError();
    if (!err.has_value())
      return nullptr;

    return DupString(*err);
  } catch (...) { return {}; }

  auto DracFreePluginFieldList(DracPluginFieldList* list) -> void try {
    if (!list || !list->items)
      return;

    for (DracPluginField& item : Span(list->items, list->count)) {
      delete[] item.key;
      FreePluginFieldValue(item.value);
    }

    delete[] list->items;
    list->items = nullptr;
    list->count = 0;
  } catch (...) { return; }

  auto DracFreePluginInfoList(DracPluginInfoList* list) -> void try {
    if (!list || !list->items)
      return;

    for (const DracPluginInfo& item : Span(list->items, list->count)) {
      delete[] item.name;
      delete[] item.version;
      delete[] item.author;
      delete[] item.description;
    }

    delete[] list->items;
    list->items = nullptr;
    list->count = 0;
  } catch (...) { return; }
#else
  // Stub implementations when plugins are disabled
  struct DracPlugin {
    int dummy;
  };

  auto DracInitStaticPlugins(void) -> size_t try {
    return 0;
  } catch (...) { return {}; }
  auto DracInitPluginManager(void) -> void try {
  } catch (...) { return; }
  auto DracShutdownPluginManager(void) -> void try {
  } catch (...) { return; }
  auto DracAddPluginSearchPath(const char* /*unused*/) -> void try {
  } catch (...) { return; }

  auto DracDiscoverPlugins(void) -> DracPluginInfoList try {
    return { nullptr, 0 };
  } catch (...) { return {}; }

  auto DracFreePluginInfoList(DracPluginInfoList* list) -> void try {
    if (list) {
      list->items = nullptr;
      list->count = 0;
    }
  } catch (...) { return; }

  auto DracLoadPlugin(const char* /*unused*/) -> DracPlugin* try {
    return nullptr;
  } catch (...) { return {}; }

  auto DracLoadPluginFromPath(const char* /*unused*/) -> DracPlugin* try {
    return nullptr;
  } catch (...) { return {}; }

  auto DracUnloadPlugin(DracPlugin* /*unused*/) -> void try {
  } catch (...) { return; }

  auto DracPluginInitialize(DracPlugin* /*unused*/, DracCacheManager* /*unused*/) -> DracErrorCode try {
    return DRAC_ERROR_NOT_SUPPORTED;
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracPluginSetConfig(DracPlugin* /*unused*/, const char* /*unused*/) -> DracErrorCode try {
    return DRAC_ERROR_NOT_SUPPORTED;
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracPluginIsEnabled(DracPlugin* /*unused*/) -> bool try {
    return false;
  } catch (...) { return {}; }

  auto DracPluginIsReady(DracPlugin* /*unused*/) -> bool try {
    return false;
  } catch (...) { return {}; }

  auto DracPluginCollectData(DracPlugin* /*unused*/, DracCacheManager* /*unused*/) -> DracErrorCode try {
    return DRAC_ERROR_NOT_SUPPORTED;
  } catch (const std::bad_alloc&) {
    return DRAC_ERROR_OUT_OF_MEMORY;
  } catch (const draconis::utils::error::DracError& error) {
    return TO_C_ERROR(error);
  } catch (...) {
    return DRAC_ERROR_INTERNAL_ERROR;
  }

  auto DracPluginGetFields(DracPlugin* /*unused*/) -> DracPluginFieldList try {
    return { nullptr, 0 };
  } catch (...) { return {}; }

  auto DracPluginGetLastError(DracPlugin* /*unused*/) -> char* try {
    return nullptr;
  } catch (...) { return {}; }

  auto DracFreePluginFieldList(DracPluginFieldList* list) -> void try {
    if (list) {
      list->items = nullptr;
      list->count = 0;
    }
  } catch (...) { return; }
#endif
}
