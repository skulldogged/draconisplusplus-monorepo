local drac = require("draconis")

print("Uptime (s):", drac.get_uptime())

local sys = drac.SystemInfo.new()

local mem = sys:get_mem_info()
print("Mem used/total:", mem.used_bytes, mem.total_bytes)

local cpu = sys:get_cpu_cores()
print("CPU cores:", cpu.physical, cpu.logical)

local os = sys:get_os()
print("OS:", os.name, os.version, os.id)

print("Host:", sys:get_host())
print("CPU model:", sys:get_cpu_model())

for i, disk in ipairs(sys:get_disks()) do
  print(("Disk %d: %s %s %d/%d"):format(
    i, disk.name, disk.mount_point, disk.used_bytes, disk.total_bytes
  ))
end

local bat = sys:get_battery()
print("Battery status:", bat.status, "percent:", bat.percentage)

