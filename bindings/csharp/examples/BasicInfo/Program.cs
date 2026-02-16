using Draconis;

try
{
    using var drac = new DraconisClient();

    Console.WriteLine($"Uptime: {drac.GetUptimeSeconds()}s");

    var cores = drac.GetCpuCores();
    Console.WriteLine($"CPU cores: {cores.Physical} physical, {cores.Logical} logical");

    var os = drac.GetOperatingSystem();
    Console.WriteLine($"OS: {os.Name} {os.Version} ({os.Id})");

    Console.WriteLine($"Host: {drac.GetHost() ?? "n/a"}");
    Console.WriteLine($"CPU model: {drac.GetCpuModel() ?? "n/a"}");
    Console.WriteLine($"GPU model: {drac.GetGpuModel() ?? "n/a"}");

    var mem = drac.GetMemoryUsage();
    Console.WriteLine($"Memory: {mem.UsedBytes} / {mem.TotalBytes} bytes");

    var disk = drac.GetDiskUsage();
    Console.WriteLine($"Disk: {disk.UsedBytes} / {disk.TotalBytes} bytes");

    var battery = drac.GetBatteryInfo();
    Console.WriteLine($"Battery: {battery.Status}, {battery.Percentage?.ToString() ?? "n/a"}%, {battery.TimeRemainingSecs?.ToString() ?? "n/a"}s remaining");
}
catch (DraconisException ex)
{
    Console.Error.WriteLine(ex.Message);
    Environment.ExitCode = 1;
}

