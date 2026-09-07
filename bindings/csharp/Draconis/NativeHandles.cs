using Microsoft.Win32.SafeHandles;

namespace Draconis;

internal sealed class CacheManagerHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    public CacheManagerHandle() : base(true) { }
    protected override bool ReleaseHandle()
    {
        NativeMethods.DracDestroyCacheManager(handle);
        return true;
    }
}

internal sealed class PluginHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    public PluginHandle() : base(true) { }
    protected override bool ReleaseHandle()
    {
        NativeMethods.DracUnloadPlugin(handle);
        return true;
    }
}
