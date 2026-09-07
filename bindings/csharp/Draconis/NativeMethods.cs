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
internal struct DracPluginFieldValueArray
{
    public IntPtr Items;
    public nuint Count;
}

internal struct DracPluginFieldValueObject
{
    public IntPtr Items;
    public nuint Count;
}

internal enum DracPluginFieldValueType
{
    Bool,
    I64,
    U64,
    F64,
    String,
    Array,
    Object,
}

[StructLayout(LayoutKind.Explicit)]
internal struct DracPluginFieldValueUnion
{
    [FieldOffset(0)] public byte BoolValue;
    [FieldOffset(0)] public long I64Value;
    [FieldOffset(0)] public ulong U64Value;
    [FieldOffset(0)] public double F64Value;
    [FieldOffset(0)] public IntPtr StringValue;
    [FieldOffset(0)] public DracPluginFieldValueArray ArrayValue;
    [FieldOffset(0)] public DracPluginFieldValueObject ObjectValue;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracPluginFieldValue
{
    public DracPluginFieldValueType Type;
    public DracPluginFieldValueUnion Value;
}

[StructLayout(LayoutKind.Sequential)]
internal struct DracPluginField
{
    public IntPtr Key;
    public DracPluginFieldValue Value;
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
    internal static extern CacheManagerHandle DracCreateCacheManager();

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
    internal static extern DracErrorCode DracGetMemInfo(CacheManagerHandle mgr, out DracResourceUsage usage);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetCpuCores(CacheManagerHandle mgr, out DracCPUCores cores);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetOperatingSystem(CacheManagerHandle mgr, out DracOSInfo info);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetDesktopEnvironment(CacheManagerHandle mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetWindowManager(CacheManagerHandle mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetShell(CacheManagerHandle mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetHost(CacheManagerHandle mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetCPUModel(CacheManagerHandle mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetGPUModel(CacheManagerHandle mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetKernelVersion(CacheManagerHandle mgr, out IntPtr str);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetDiskUsage(CacheManagerHandle mgr, out DracResourceUsage usage);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetDisks(CacheManagerHandle mgr, out DracDiskInfoList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetSystemDisk(CacheManagerHandle mgr, out DracDiskInfo info);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetOutputs(CacheManagerHandle mgr, out DracDisplayInfoList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetPrimaryOutput(CacheManagerHandle mgr, out DracDisplayInfo info);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetNetworkInterfaces(CacheManagerHandle mgr, out DracNetworkInterfaceList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetPrimaryNetworkInterface(CacheManagerHandle mgr, out DracNetworkInterface iface);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracGetBatteryInfo(CacheManagerHandle mgr, out DracBattery battery);

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
    internal static extern PluginHandle DracLoadPlugin([MarshalAs(UnmanagedType.LPStr)] string pluginName);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern PluginHandle DracLoadPluginFromPath([MarshalAs(UnmanagedType.LPStr)] string path);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracUnloadPlugin(IntPtr plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracPluginInitialize(PluginHandle plugin, CacheManagerHandle cache);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static extern bool DracPluginIsEnabled(PluginHandle plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static extern bool DracPluginIsReady(PluginHandle plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracErrorCode DracPluginCollectData(PluginHandle plugin, CacheManagerHandle cache);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern DracPluginFieldList DracPluginGetFields(PluginHandle plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr DracPluginGetLastError(PluginHandle plugin);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreePluginInfoList(ref DracPluginInfoList list);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void DracFreePluginFieldList(ref DracPluginFieldList list);
}
