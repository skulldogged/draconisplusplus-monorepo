#include <stdio.h>

#include "../include/draconis_c.h"

int main() {
  DracCacheManager* mgr = DracCreateCacheManager();

  if (!mgr) {
    (void)fprintf(stderr, "Failed to create cache manager!\n");
    return 1;
  }

  printf("=== System Information ===\n\n");

  uint64_t uptime = DracGetUptime();
  printf("Uptime: %llu seconds\n", uptime);

  DracResourceUsage memInfo;

  if (DracGetMemInfo(mgr, &memInfo) == DRAC_SUCCESS) {
    printf(
      "Memory: %llu / %llu bytes used\n",
      memInfo.usedBytes,
      memInfo.totalBytes
    );
  } else {
    printf("Failed to get memory info\n");
  }

  DracCPUCores cores;

  if (DracGetCpuCores(mgr, &cores) == DRAC_SUCCESS) {
    printf(
      "CPU Cores: %zu physical, %zu logical\n",
      cores.physical,
      cores.logical
    );
  } else {
    printf("Failed to get CPU cores\n");
  }

  DracOSInfo osInfo;

  if (DracGetOperatingSystem(mgr, &osInfo) == DRAC_SUCCESS) {
    printf("OS: %s %s (%s)\n", osInfo.name, osInfo.version, osInfo.id);
    DracFreeOSInfo(&osInfo);
  } else {
    printf("Failed to get OS info\n");
  }

  char* shell = NULL;

  if (DracGetShell(mgr, &shell) == DRAC_SUCCESS) {
    printf("Shell: %s\n", shell);
    DracFreeString(shell);
  } else {
    printf("Shell: N/A\n");
  }

  char* host = NULL;

  if (DracGetHost(mgr, &host) == DRAC_SUCCESS) {
    printf("Host: %s\n", host);
    DracFreeString(host);
  } else {
    printf("Host: N/A\n");
  }

  char* cpuModel = NULL;

  if (DracGetCPUModel(mgr, &cpuModel) == DRAC_SUCCESS) {
    printf("CPU Model: %s\n", cpuModel);
    DracFreeString(cpuModel);
  } else {
    printf("CPU Model: N/A\n");
  }

  char* gpuModel = NULL;

  if (DracGetGPUModel(mgr, &gpuModel) == DRAC_SUCCESS) {
    printf("GPU Model: %s\n", gpuModel);
    DracFreeString(gpuModel);
  } else {
    printf("GPU Model: N/A\n");
  }

  char* kernel = NULL;

  if (DracGetKernelVersion(mgr, &kernel) == DRAC_SUCCESS) {
    printf("Kernel: %s\n", kernel);
    DracFreeString(kernel);
  } else {
    printf("Kernel: N/A\n");
  }

  char* desktopEnv = NULL;

  if (DracGetDesktopEnvironment(mgr, &desktopEnv) == DRAC_SUCCESS) {
    printf("Desktop Environment: %s\n", desktopEnv);
    DracFreeString(desktopEnv);
  } else {
    printf("Desktop Environment: N/A\n");
  }

  char* winMgr = NULL;

  if (DracGetWindowManager(mgr, &winMgr) == DRAC_SUCCESS) {
    printf("Window Manager: %s\n", winMgr);
    DracFreeString(winMgr);
  } else {
    printf("Window Manager: N/A\n");
  }

  printf("\n=== Storage ===\n\n");

  DracResourceUsage diskUsage;

  if (DracGetDiskUsage(mgr, &diskUsage) == DRAC_SUCCESS) {
    printf(
      "Total Disk Usage: %llu / %llu bytes\n",
      diskUsage.usedBytes,
      diskUsage.totalBytes
    );
  } else {
    printf("Failed to get disk usage\n");
  }

  DracDiskInfo sysDisk;

  if (DracGetSystemDisk(mgr, &sysDisk) == DRAC_SUCCESS) {
    printf(
      "System Disk: %s (%s) at %s - %llu / %llu bytes\n",
      sysDisk.name,
      sysDisk.filesystem,
      sysDisk.mountPoint,
      sysDisk.usedBytes,
      sysDisk.totalBytes
    );
    DracFreeDiskInfo(&sysDisk);
  } else {
    printf("Failed to get system disk\n");
  }

  DracDiskInfoList disks;

  if (DracGetDisks(mgr, &disks) == DRAC_SUCCESS) {
    printf("Found %zu disks:\n", disks.count);

    for (size_t i = 0; i < disks.count; ++i) {
      DracDiskInfo* disk = &disks.items[i];

      printf(
        "  - %s (%s) at %s: %llu / %llu bytes%s\n",
        disk->name,
        disk->filesystem,
        disk->mountPoint,
        disk->usedBytes,
        disk->totalBytes,
        disk->isSystemDrive ? " [SYSTEM]" : ""
      );
    }
    DracFreeDiskInfoList(&disks);
  } else {
    printf("Failed to get disks\n");
  }

  printf("\n=== Displays ===\n\n");

  DracDisplayInfo primaryDisplay;

  if (DracGetPrimaryOutput(mgr, &primaryDisplay) == DRAC_SUCCESS) {
    printf(
      "Primary Display: %llux%llu @ %.2f Hz\n",
      primaryDisplay.width,
      primaryDisplay.height,
      primaryDisplay.refreshRate
    );
  } else {
    printf("Failed to get primary display\n");
  }

  DracDisplayInfoList displays;

  if (DracGetOutputs(mgr, &displays) == DRAC_SUCCESS) {
    printf("Found %zu displays:\n", displays.count);

    for (size_t i = 0; i < displays.count; ++i) {
      DracDisplayInfo* display = &displays.items[i];

      printf(
        "  - Display %llu: %llux%llu @ %.2f Hz%s\n",
        display->id,
        display->width,
        display->height,
        display->refreshRate,
        display->isPrimary ? " [PRIMARY]" : ""
      );
    }

    DracFreeDisplayInfoList(&displays);
  } else {
    printf("Failed to get displays\n");
  }

  printf("\n=== Network ===\n\n");

  DracNetworkInterface primaryNet;

  if (DracGetPrimaryNetworkInterface(mgr, &primaryNet) == DRAC_SUCCESS) {
    printf(
      "Primary Interface: %s (IPv4: %s)\n",
      primaryNet.name,
      primaryNet.ipv4Address ? primaryNet.ipv4Address : "N/A"
    );
    DracFreeNetworkInterface(&primaryNet);
  } else {
    printf("Failed to get primary network interface\n");
  }

  DracNetworkInterfaceList netIfaces;

  if (DracGetNetworkInterfaces(mgr, &netIfaces) == DRAC_SUCCESS) {
    printf("Found %zu network interfaces:\n", netIfaces.count);

    for (size_t i = 0; i < netIfaces.count; ++i) {
      DracNetworkInterface* iface = &netIfaces.items[i];

      printf(
        "  - %s: IPv4=%s IPv6=%s MAC=%s up=%s loopback=%s\n",
        iface->name,
        iface->ipv4Address ? iface->ipv4Address : "N/A",
        iface->ipv6Address ? iface->ipv6Address : "N/A",
        iface->macAddress ? iface->macAddress : "N/A",
        iface->isUp ? "yes" : "no",
        iface->isLoopback ? "yes" : "no"
      );
    }

    DracFreeNetworkInterfaceList(&netIfaces);
  } else {
    printf("Failed to get network interfaces\n");
  }

  printf("\n=== Battery ===\n\n");

  DracBattery battery;

  if (DracGetBatteryInfo(mgr, &battery) == DRAC_SUCCESS) {
    const char* statusStr = "Unknown";

    switch (battery.status) {
      case DRAC_BATTERY_CHARGING:    statusStr = "Charging"; break;
      case DRAC_BATTERY_DISCHARGING: statusStr = "Discharging"; break;
      case DRAC_BATTERY_FULL:        statusStr = "Full"; break;
      case DRAC_BATTERY_NOT_PRESENT: statusStr = "Not Present"; break;
      default:                       break;
    }

    printf("Battery Status: %s\n", statusStr);

    if (battery.percentage != UINT8_MAX)
      printf("Battery Percentage: %u%%\n", battery.percentage);

    if (battery.timeRemainingSecs >= 0)
      printf("Time Remaining: %lld seconds\n", battery.timeRemainingSecs);
  } else {
    printf("Battery: N/A\n");
  }

  printf("\n");

  DracDestroyCacheManager(mgr);
  return 0;
}
