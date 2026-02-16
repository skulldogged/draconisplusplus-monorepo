module demo;

import c3::draconis;
import c3::draconis_raw;
import libc;

fn int main() {
  c3::draconis::CacheManager mgr = c3::draconis::create_manager();
  if (mgr == null) {
    libc::printf("Failed to create cache manager!\n");
    return 1;
  }
  defer c3::draconis::destroy_manager(mgr);

  libc::printf("=== System Information (C3) ===\n\n");

  ulong uptime = c3::draconis::get_uptime();
  libc::printf("Uptime: %llu seconds\n", uptime);

  draconis_raw::DracResourceUsage mem = (draconis_raw::DracResourceUsage){};
  if (c3::draconis::ok(c3::draconis::get_mem_info(mgr, &mem))) {
    libc::printf("Memory: %llu / %llu bytes used\n", mem.usedBytes, mem.totalBytes);
  }

  draconis_raw::DracCPUCores cores = (draconis_raw::DracCPUCores){};
  if (c3::draconis::ok(c3::draconis::get_cpu_cores(mgr, &cores))) {
    libc::printf("CPU Cores: %zu physical, %zu logical\n", cores.physical, cores.logical);
  }

  draconis_raw::DracOSInfo os = (draconis_raw::DracOSInfo){};
  if (c3::draconis::ok(c3::draconis::get_operating_system(mgr, &os))) {
    libc::printf("OS: %s %s (%s)\n", os.name, os.version, os.id);
    c3::draconis::free_os_info(&os);
  }

  char* shell = c3::draconis::get_shell(mgr);
  if (shell != null) {
    libc::printf("Shell: %s\n", shell);
    c3::draconis::free_string(shell);
  }

  return 0;
}
