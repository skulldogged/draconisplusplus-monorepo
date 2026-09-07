using Draconis;

using var client = new DraconisClient();
if (client.GetMemoryUsage().TotalBytes == 0 || client.GetCpuCores().Logical == 0)
    throw new Exception("Invalid native system information");
if (args.Length > 0)
{
    using var plugin = Plugin.LoadFromPath(args[0]) ?? throw new Exception("Fixture did not load");
    plugin.Initialize(client);
    PluginSystem.Shutdown();
    if (!plugin.IsReady) throw new Exception("Retained handle lost readiness");
    plugin.CollectData(client);
    plugin.Dispose();
    plugin.Dispose();
    try { _ = plugin.IsReady; throw new Exception("Closed plugin accepted a call"); }
    catch (ObjectDisposedException) { }
}
client.Dispose();
client.Dispose();
try { client.GetMemoryUsage(); throw new Exception("Closed client accepted a call"); }
catch (ObjectDisposedException) { }
Console.WriteLine("C# native ownership smoke passed");
