#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__HAIKU__) || defined(__serenity__)
  #include <Drac++/Core/System.hpp>

  #include <Drac++/Utils/Error.hpp>
namespace draconis::core::system {
  using namespace utils::types;
  using utils::cache::CacheManager;
  using enum utils::error::DracErrorCode;
  auto GetCPUModel(CacheManager&) -> Result<String> {
    ERR(NotSupported, "GetCPUModel is not implemented for this platform");
  }
  auto GetCPUCores(CacheManager&) -> Result<CPUCores> {
    ERR(NotSupported, "GetCPUCores is not implemented for this platform");
  }
  auto GetGPUModel(CacheManager&) -> Result<String> {
    ERR(NotSupported, "GetGPUModel is not implemented for this platform");
  }
  auto GetOutputs(CacheManager&) -> Result<Vec<DisplayInfo>> {
    ERR(NotSupported, "GetOutputs is not implemented for this platform");
  }
  auto GetPrimaryOutput(CacheManager&) -> Result<DisplayInfo> {
    ERR(NotSupported, "GetPrimaryOutput is not implemented for this platform");
  }
  auto GetNetworkInterfaces(CacheManager&) -> Result<Vec<NetworkInterface>> {
    ERR(NotSupported, "GetNetworkInterfaces is not implemented for this platform");
  }
  auto GetPrimaryNetworkInterface(CacheManager&) -> Result<NetworkInterface> {
    ERR(NotSupported, "GetPrimaryNetworkInterface is not implemented for this platform");
  }
  auto GetBatteryInfo(CacheManager&) -> Result<Battery> {
    ERR(NotSupported, "GetBatteryInfo is not implemented for this platform");
  }
  auto GetUptime() -> Result<std::chrono::seconds> {
    ERR(NotSupported, "Uptime is not implemented for this platform");
  }
  #ifdef __OpenBSD__
  auto GetMemInfo(CacheManager&) -> Result<ResourceUsage> {
    ERR(NotSupported, "GetMemInfo is not implemented for this platform");
  }
  auto GetOperatingSystem(CacheManager&) -> Result<OSInfo> {
    ERR(NotSupported, "GetOperatingSystem is not implemented for this platform");
  }
  auto GetDesktopEnvironment(CacheManager&) -> Result<String> {
    ERR(NotSupported, "GetDesktopEnvironment is not implemented for this platform");
  }
  auto GetWindowManager(CacheManager&) -> Result<String> {
    ERR(NotSupported, "GetWindowManager is not implemented for this platform");
  }
  auto GetShell(CacheManager&) -> Result<String> {
    ERR(NotSupported, "GetShell is not implemented for this platform");
  }
  auto GetHost(CacheManager&) -> Result<String> {
    ERR(NotSupported, "GetHost is not implemented for this platform");
  }
  auto GetKernelVersion(CacheManager&) -> Result<String> {
    ERR(NotSupported, "GetKernelVersion is not implemented for this platform");
  }
  auto GetDiskUsage(CacheManager&) -> Result<ResourceUsage> {
    ERR(NotSupported, "GetDiskUsage is not implemented for this platform");
  }
  auto GetDisks(CacheManager&) -> Result<Vec<DiskInfo>> {
    ERR(NotSupported, "GetDisks is not implemented for this platform");
  }
  auto GetSystemDisk(CacheManager&) -> Result<DiskInfo> {
    ERR(NotSupported, "GetSystemDisk is not implemented for this platform");
  }
  auto GetDiskByPath(const String&, CacheManager&) -> Result<DiskInfo> {
    ERR(NotSupported, "Disk lookup is not implemented for OpenBSD");
  }
  #endif
} // namespace draconis::core::system
#endif
