using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Draconis;

public sealed class DraconisClient : IDisposable
{
    private readonly IntPtr _mgr;
    private bool _disposed;

    public DraconisClient()
    {
        _mgr = NativeMethods.DracCreateCacheManager();
        if (_mgr == IntPtr.Zero)
            throw new InvalidOperationException("Failed to create native CacheManager.");
    }

    public ulong GetUptimeSeconds() => NativeMethods.DracGetUptime();

    public ResourceUsage GetMemoryUsage()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetMemInfo(_mgr, out var usage);
        ThrowIfError(code);
        return new ResourceUsage(usage.UsedBytes, usage.TotalBytes);
    }

    public CpuCores GetCpuCores()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetCpuCores(_mgr, out var cores);
        ThrowIfError(code);
        return new CpuCores((ulong)cores.Physical, (ulong)cores.Logical);
    }

    public OsInfo GetOperatingSystem()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetOperatingSystem(_mgr, out var info);
        ThrowIfError(code);
        try
        {
            return new OsInfo(
                TakeString(info.Name, free: false),
                TakeString(info.Version, free: false),
                TakeString(info.Id, free: false)
            );
        }
        finally
        {
            NativeMethods.DracFreeOSInfo(ref info);
        }
    }

    public string? GetDesktopEnvironment() => GetString(NativeMethods.DracGetDesktopEnvironment);
    public string? GetWindowManager() => GetString(NativeMethods.DracGetWindowManager);
    public string? GetShell() => GetString(NativeMethods.DracGetShell);
    public string? GetHost() => GetString(NativeMethods.DracGetHost);
    public string? GetCpuModel() => GetString(NativeMethods.DracGetCPUModel);
    public string? GetGpuModel() => GetString(NativeMethods.DracGetGPUModel);
    public string? GetKernelVersion() => GetString(NativeMethods.DracGetKernelVersion);

    public ResourceUsage GetDiskUsage()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetDiskUsage(_mgr, out var usage);
        ThrowIfError(code);
        return new ResourceUsage(usage.UsedBytes, usage.TotalBytes);
    }

    public IReadOnlyList<DiskInfo> GetDisks()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetDisks(_mgr, out var list);
        ThrowIfError(code);
        try
        {
            return ReadArray<DracDiskInfo, DiskInfo>(
                list.Items,
                list.Count,
                native =>
                {
                    var info = new DiskInfo(
                        TakeString(native.Name, free: false),
                        TakeString(native.MountPoint, free: false),
                        TakeString(native.Filesystem, free: false),
                        TakeString(native.DriveType, free: false),
                        native.TotalBytes,
                        native.UsedBytes,
                        native.IsSystemDrive
                    );
                    return info;
                });
        }
        finally
        {
            NativeMethods.DracFreeDiskInfoList(ref list);
        }
    }

    public DiskInfo GetSystemDisk()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetSystemDisk(_mgr, out var native);
        ThrowIfError(code);
        try
        {
            return new DiskInfo(
                TakeString(native.Name, free: false),
                TakeString(native.MountPoint, free: false),
                TakeString(native.Filesystem, free: false),
                TakeString(native.DriveType, free: false),
                native.TotalBytes,
                native.UsedBytes,
                native.IsSystemDrive
            );
        }
        finally
        {
            NativeMethods.DracFreeDiskInfo(ref native);
        }
    }

    public IReadOnlyList<DisplayInfo> GetOutputs()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetOutputs(_mgr, out var list);
        ThrowIfError(code);
        try
        {
            return ReadArray<DracDisplayInfo, DisplayInfo>(
                list.Items,
                list.Count,
                native => new DisplayInfo(
                    native.Id,
                    native.Width,
                    native.Height,
                    native.RefreshRate,
                    native.IsPrimary
                ));
        }
        finally
        {
            NativeMethods.DracFreeDisplayInfoList(ref list);
        }
    }

    public DisplayInfo GetPrimaryOutput()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetPrimaryOutput(_mgr, out var native);
        ThrowIfError(code);
        return new DisplayInfo(
            native.Id,
            native.Width,
            native.Height,
            native.RefreshRate,
            native.IsPrimary
        );
    }

    public IReadOnlyList<NetworkInterfaceInfo> GetNetworkInterfaces()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetNetworkInterfaces(_mgr, out var list);
        ThrowIfError(code);
        try
        {
            return ReadArray<DracNetworkInterface, NetworkInterfaceInfo>(
                list.Items,
                list.Count,
                native =>
                {
                    var info = new NetworkInterfaceInfo(
                        TakeString(native.Name, free: false),
                        TakeString(native.Ipv4Address, free: false),
                        TakeString(native.Ipv6Address, free: false),
                        TakeString(native.MacAddress, free: false),
                        native.IsUp,
                        native.IsLoopback
                    );
                    return info;
                });
        }
        finally
        {
            NativeMethods.DracFreeNetworkInterfaceList(ref list);
        }
    }

    public NetworkInterfaceInfo GetPrimaryNetworkInterface()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetPrimaryNetworkInterface(_mgr, out var native);
        ThrowIfError(code);
        try
        {
            return new NetworkInterfaceInfo(
                TakeString(native.Name, free: false),
                TakeString(native.Ipv4Address, free: false),
                TakeString(native.Ipv6Address, free: false),
                TakeString(native.MacAddress, free: false),
                native.IsUp,
                native.IsLoopback
            );
        }
        finally
        {
            NativeMethods.DracFreeNetworkInterface(ref native);
        }
    }

    public BatteryInfo GetBatteryInfo()
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracGetBatteryInfo(_mgr, out var battery);
        ThrowIfError(code);
        return new BatteryInfo(
            (BatteryStatus)battery.Status,
            battery.Percentage == byte.MaxValue ? null : battery.Percentage,
            battery.TimeRemainingSecs < 0 ? null : battery.TimeRemainingSecs
        );
    }

    private delegate DracErrorCode StringGetter(IntPtr mgr, out IntPtr str);

    private string? GetString(StringGetter getter)
    {
        EnsureNotDisposed();
        var code = getter(_mgr, out var ptr);
        ThrowIfError(code);
        return TakeString(ptr);
    }

    private static string? TakeString(IntPtr ptr, bool free = true)
    {
        if (ptr == IntPtr.Zero) return null;
        var str = Marshal.PtrToStringUTF8(ptr);
        if (free)
            NativeMethods.DracFreeString(ptr);
        return str;
    }

    private static IReadOnlyList<TManaged> ReadArray<TNative, TManaged>(
        IntPtr itemsPtr,
        nuint count,
        Func<TNative, TManaged> map)
        where TNative : struct
    {
        var size = Marshal.SizeOf<TNative>();
        var results = new List<TManaged>((int)count);
        for (nuint i = 0; i < count; i++)
        {
            var elementPtr = IntPtr.Add(itemsPtr, (int)(i * (nuint)size));
            var native = Marshal.PtrToStructure<TNative>(elementPtr);
            results.Add(map(native));
        }
        return results;
    }

    private static void ThrowIfError(DracErrorCode code)
    {
        if (code == DracErrorCode.Success) return;
        throw new DraconisException(code);
    }

    private void EnsureNotDisposed()
    {
        if (_disposed) throw new ObjectDisposedException(nameof(DraconisClient));
    }

    public void Dispose()
    {
        if (_disposed) return;
        NativeMethods.DracDestroyCacheManager(_mgr);
        _disposed = true;
        GC.SuppressFinalize(this);
    }
}

public sealed class DraconisException : Exception
{
    public DracErrorCode ErrorCode { get; }
    public DraconisException(DracErrorCode code)
        : base($"Draconis native call failed: {code}")
    {
        ErrorCode = code;
    }
}

public readonly record struct ResourceUsage(ulong UsedBytes, ulong TotalBytes);
public readonly record struct CpuCores(ulong Physical, ulong Logical);
public readonly record struct OsInfo(string? Name, string? Version, string? Id);
public readonly record struct DiskInfo(
    string? Name,
    string? MountPoint,
    string? Filesystem,
    string? DriveType,
    ulong TotalBytes,
    ulong UsedBytes,
    bool IsSystemDrive);
public readonly record struct DisplayInfo(
    ulong Id,
    ulong Width,
    ulong Height,
    double RefreshRate,
    bool IsPrimary);
public readonly record struct NetworkInterfaceInfo(
    string? Name,
    string? Ipv4Address,
    string? Ipv6Address,
    string? MacAddress,
    bool IsUp,
    bool IsLoopback);
public enum BatteryStatus
{
    Unknown = 0,
    Charging = 1,
    Discharging = 2,
    Full = 3,
    NotPresent = 4,
}
public readonly record struct BatteryInfo(
    BatteryStatus Status,
    byte? Percentage,
    long? TimeRemainingSecs);

// Plugin system types
public sealed class Plugin : IDisposable
{
    private IntPtr _handle;
    private bool _disposed;

    internal Plugin(IntPtr handle)
    {
        _handle = handle;
    }

    public static Plugin? Load(string pluginName)
    {
        if (string.IsNullOrEmpty(pluginName)) return null;
        var handle = NativeMethods.DracLoadPlugin(pluginName);
        return handle == IntPtr.Zero ? null : new Plugin(handle);
    }

    public static Plugin? LoadFromPath(string path)
    {
        if (string.IsNullOrEmpty(path)) return null;
        var handle = NativeMethods.DracLoadPluginFromPath(path);
        return handle == IntPtr.Zero ? null : new Plugin(handle);
    }

    public void Initialize(DraconisClient client)
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracPluginInitialize(_handle, client._mgr);
        ThrowIfError(code);
    }

    public bool IsEnabled
    {
        get
        {
            EnsureNotDisposed();
            return NativeMethods.DracPluginIsEnabled(_handle);
        }
    }

    public bool IsReady
    {
        get
        {
            EnsureNotDisposed();
            return NativeMethods.DracPluginIsReady(_handle);
        }
    }

    public void CollectData(DraconisClient client)
    {
        EnsureNotDisposed();
        var code = NativeMethods.DracPluginCollectData(_handle, client._mgr);
        ThrowIfError(code);
    }

    public string? GetJson()
    {
        EnsureNotDisposed();
        var ptr = NativeMethods.DracPluginGetJson(_handle);
        return TakeString(ptr);
    }

    public IReadOnlyDictionary<string, string> GetFields()
    {
        EnsureNotDisposed();
        var list = NativeMethods.DracPluginGetFields(_handle);
        try
        {
            var result = new Dictionary<string, string>();
            if (list.Items == IntPtr.Zero || list.Count == 0)
                return result;

            var size = Marshal.SizeOf<DracPluginField>();
            for (nuint i = 0; i < list.Count; i++)
            {
                var fieldPtr = IntPtr.Add(list.Items, (int)(i * (nuint)size));
                var field = Marshal.PtrToStructure<DracPluginField>(fieldPtr);
                var key = TakeString(field.Key, free: false) ?? "";
                var value = TakeString(field.Value, free: false) ?? "";
                result[key] = value;
            }
            return result;
        }
        finally
        {
            NativeMethods.DracFreePluginFieldList(ref list);
        }
    }

    public string? GetLastError()
    {
        EnsureNotDisposed();
        var ptr = NativeMethods.DracPluginGetLastError(_handle);
        return TakeString(ptr);
    }

    private void EnsureNotDisposed()
    {
        if (_disposed) throw new ObjectDisposedException(nameof(Plugin));
    }

    public void Dispose()
    {
        if (_disposed) return;
        if (_handle != IntPtr.Zero)
        {
            NativeMethods.DracUnloadPlugin(_handle);
            _handle = IntPtr.Zero;
        }
        _disposed = true;
        GC.SuppressFinalize(this);
    }
}

public static class PluginSystem
{
    public static nuint InitStaticPlugins() => NativeMethods.DracInitStaticPlugins();

    public static void Initialize() => NativeMethods.DracInitPluginManager();

    public static void Shutdown() => NativeMethods.DracShutdownPluginManager();

    public static void AddSearchPath(string path) => NativeMethods.DracAddPluginSearchPath(path);
}
