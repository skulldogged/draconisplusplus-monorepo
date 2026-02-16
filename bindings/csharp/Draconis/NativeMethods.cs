using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Draconis;

public enum DracErrorCode : int
{
    ApiUnavailable = 0,
    ConfigurationError = 1,
    CorruptedData = 2,
    InternalError = 3,
    InvalidArgument = 4,
    IoError = 5,
    NetworkError = 6,
    NotFound = 7,
    NotSupported = 8,
    Other = 9,
    OutOfMemory = 10,
    ParseError = 11,
    PermissionDenied = 12,
    PermissionRequired = 13,
    PlatformSpecific = 14,
    ResourceExhausted = 15,
    Timeout = 16,
    UnavailableFeature = 17,
    Success = 255,
}

internal enum DracBatteryStatus : int
{
    Unknown = 0,
    Charging = 1,
    Discharging = 2,
    Full = 3,
    NotPresent = 4,
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracResourceUsage
{
    public ulong UsedBytes;
    public ulong TotalBytes;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracCPUCores
{
    public nuint Physical;
    public nuint Logical;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracOSInfo
{
    public IntPtr Name;
    public IntPtr Version;
    public IntPtr Id;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracDiskInfo
{
    public IntPtr Name;
    public IntPtr MountPoint;
    public IntPtr Filesystem;
    public IntPtr DriveType;
    public ulong TotalBytes;
    public ulong UsedBytes;
    [MarshalAs(UnmanagedType.I1)]
    public bool IsSystemDrive;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracDiskInfoList
{
    public IntPtr Items;
    public nuint Count;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracDisplayInfo
{
    public ulong Id;
    public ulong Width;
    public ulong Height;
    public double RefreshRate;
    [MarshalAs(UnmanagedType.I1)]
    public bool IsPrimary;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracDisplayInfoList
{
    public IntPtr Items;
    public nuint Count;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracNetworkInterface
{
    public IntPtr Name;
    public IntPtr Ipv4Address;
    public IntPtr Ipv6Address;
    public IntPtr MacAddress;
    [MarshalAs(UnmanagedType.I1)]
    public bool IsUp;
    [MarshalAs(UnmanagedType.I1)]
    public bool IsLoopback;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracNetworkInterfaceList
{
    public IntPtr Items;
    public nuint Count;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracBattery
{
    public DracBatteryStatus Status;
    public byte Percentage;
    public long TimeRemainingSecs;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracPluginInfo
{
    public IntPtr Name;
    public IntPtr Version;
    public IntPtr Author;
    public IntPtr Description;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracPluginInfoList
{
    public IntPtr Items;
    public nuint Count;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracPluginField
{
    public IntPtr Key;
    public IntPtr Value;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracPluginFieldList
{
    public IntPtr Items;
    public nuint Count;
}

internal static class NativeMethods
{
    private const string LibraryName = "draconis_c";

    static NativeMethods()
    {
        NativeLibrary.SetDllImportResolver(typeof(NativeMethods).Assembly, ResolveLibrary);
    }

    private static IntPtr ResolveLibrary(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (!string.Equals(libraryName, LibraryName, StringComparison.Ordinal))
            return IntPtr.Zero;

        var overridePath = Environment.GetEnvironmentVariable("DRACONIS_C_LIBRARY");
        if (!string.IsNullOrWhiteSpace(overridePath) && File.Exists(overridePath))
            return NativeLibrary.Load(overridePath);

        var baseDir = AppContext.BaseDirectory;
        var candidates = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? new[] { "draconis_c.dll" }
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? new[] { "libdraconis_c.dylib", "draconis_c.dylib" }
                : new[] { "libdraconis_c.so", "draconis_c.so" };

        foreach (var name in candidates)
        {
            var path = Path.Combine(baseDir, name);
            if (File.Exists(path))
                return NativeLibrary.Load(path);
        }

        return IntPtr.Zero;
    }

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr DracCreateCacheManager();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracDestroyCacheManager(IntPtr mgr);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreeString(IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreeOSInfo(ref DracOSInfo info);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreeDiskInfo(ref DracDiskInfo info);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreeDiskInfoList(ref DracDiskInfoList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreeDisplayInfoList(ref DracDisplayInfoList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreeNetworkInterface(ref DracNetworkInterface iface);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreeNetworkInterfaceList(ref DracNetworkInterfaceList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern ulong DracGetUptime();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetMemInfo(IntPtr mgr, out DracResourceUsage usage);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetCpuCores(IntPtr mgr, out DracCPUCores cores);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetOperatingSystem(IntPtr mgr, out DracOSInfo info);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetDesktopEnvironment(IntPtr mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetWindowManager(IntPtr mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetShell(IntPtr mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetHost(IntPtr mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetCPUModel(IntPtr mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetGPUModel(IntPtr mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetKernelVersion(IntPtr mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetDiskUsage(IntPtr mgr, out DracResourceUsage usage);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetDisks(IntPtr mgr, out DracDiskInfoList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetSystemDisk(IntPtr mgr, out DracDiskInfo info);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetOutputs(IntPtr mgr, out DracDisplayInfoList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetPrimaryOutput(IntPtr mgr, out DracDisplayInfo info);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetNetworkInterfaces(IntPtr mgr, out DracNetworkInterfaceList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetPrimaryNetworkInterface(IntPtr mgr, out DracNetworkInterface iface);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetBatteryInfo(IntPtr mgr, out DracBattery battery);

    // Plugin system
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern nuint DracInitStaticPlugins();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracInitPluginManager();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracShutdownPluginManager();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracAddPluginSearchPath([MarshalAs(UnmanagedType.LPStr)] string path);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracPluginInfoList DracDiscoverPlugins();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr DracLoadPlugin([MarshalAs(UnmanagedType.LPStr)] string pluginName);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr DracLoadPluginFromPath([MarshalAs(UnmanagedType.LPStr)] string path);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracUnloadPlugin(IntPtr plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracPluginInitialize(IntPtr plugin, IntPtr cache);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static extern bool DracPluginIsEnabled(IntPtr plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static extern bool DracPluginIsReady(IntPtr plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracPluginCollectData(IntPtr plugin, IntPtr cache);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr DracPluginGetJson(IntPtr plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracPluginFieldList DracPluginGetFields(IntPtr plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr DracPluginGetLastError(IntPtr plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreePluginInfoList(ref DracPluginInfoList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreePluginFieldList(ref DracPluginFieldList list);
}
