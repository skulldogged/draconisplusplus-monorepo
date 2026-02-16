#!/usr/bin/env python3
"""Demo script showing draconis Python bindings usage."""

import draconis

def format_bytes(n: int) -> str:
    """Format bytes as human-readable string."""
    for unit in ["B", "KB", "MB", "GB", "TB"]:
        if n < 1024:
            return f"{n:.1f} {unit}"
        n /= 1024
    return f"{n:.1f} PB"

def main() -> None:
    # Create system info instance (caches repeated queries)
    system = draconis.SystemInfo()

    # Basic system info
    print(f"Uptime: {draconis.SystemInfo.get_uptime()} seconds")
    print(f"Host: {system.get_host()}")
    print(f"Shell: {system.get_shell()}")

    # OS info
    os_info = system.get_os()
    print(f"OS: {os_info.name} {os_info.version} ({os_info.id})")
    print(f"Kernel: {system.get_kernel_version()}")

    # Try desktop environment (may not be available on all systems)
    try:
        print(f"Desktop: {system.get_desktop_environment()}")
    except RuntimeError:
        print("Desktop: N/A")

    try:
        print(f"Window Manager: {system.get_window_manager()}")
    except RuntimeError:
        print("Window Manager: N/A")

    # CPU info
    print(f"\nCPU: {system.get_cpu_model()}")
    cores = system.get_cpu_cores()
    print(f"Cores: {cores.physical} physical, {cores.logical} logical")

    # GPU info
    try:
        print(f"GPU: {system.get_gpu_model()}")
    except RuntimeError:
        print("GPU: N/A")

    # Memory info
    mem = system.get_mem_info()
    print(f"\nMemory: {format_bytes(mem.used_bytes)} / {format_bytes(mem.total_bytes)}")

    # Disk info
    disk_usage = system.get_disk_usage()
    print(f"Disk: {format_bytes(disk_usage.used_bytes)} / {format_bytes(disk_usage.total_bytes)}")

    print("\nDisks:")
    for disk in system.get_disks():
        sys_marker = " [System]" if disk.is_system_drive else ""
        print(f"  {disk.name} ({disk.mount_point}) - {disk.filesystem}{sys_marker}")

    # Display info
    print("\nDisplays:")
    for output in system.get_outputs():
        primary = " [Primary]" if output.is_primary else ""
        print(f"  Display {output.id}: {output.width}x{output.height}@{output.refresh_rate:.0f}Hz{primary}")

    # Network info
    print("\nNetwork Interfaces:")
    for iface in system.get_network_interfaces():
        status = "up" if iface.is_up else "down"
        loopback = " (loopback)" if iface.is_loopback else ""
        print(f"  {iface.name}: {status}{loopback}")
        if iface.ipv4_address:
            print(f"    IPv4: {iface.ipv4_address}")
        if iface.ipv6_address:
            print(f"    IPv6: {iface.ipv6_address}")
        if iface.mac_address:
            print(f"    MAC: {iface.mac_address}")

    # Battery info
    try:
        battery = system.get_battery_info()
        status_names = {
            draconis.BatteryStatus.Unknown: "Unknown",
            draconis.BatteryStatus.Charging: "Charging",
            draconis.BatteryStatus.Discharging: "Discharging",
            draconis.BatteryStatus.Full: "Full",
            draconis.BatteryStatus.NotPresent: "Not Present",
        }
        print(f"\nBattery: {status_names.get(battery.status, 'Unknown')}")
        if battery.percentage is not None:
            print(f"  Percentage: {battery.percentage}%")
        if battery.time_remaining_secs is not None:
            mins = battery.time_remaining_secs // 60
            print(f"  Time remaining: {mins} minutes")
    except RuntimeError:
        print("\nBattery: N/A")

if __name__ == "__main__":
    main()
