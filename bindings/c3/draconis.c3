module c3::draconis;

import c3::draconis_raw;

// Thin ergonomic layer over the raw C API.

alias CacheManager = DracCacheManager*;
alias ErrorCode = DracErrorCode;
alias BatteryStatus = DracBatteryStatus;

fn bool ok(ErrorCode code) {
  return code == draconis_raw::DRAC_SUCCESS;
}

fn CacheManager create_manager() {
  return draconis_raw::drac_create_cache_manager();
}

fn void destroy_manager(CacheManager mgr) {
  if (mgr != null) {
    draconis_raw::drac_destroy_cache_manager(mgr);
  }
}

fn void free_string(char* s) {
  if (s != null) {
    draconis_raw::drac_free_string(s);
  }
}

fn void free_os_info(DracOSInfo* info) {
  if (info != null) {
    draconis_raw::drac_free_os_info(info);
  }
}

fn void free_disk_info(DracDiskInfo* info) {
  if (info != null) {
    draconis_raw::drac_free_disk_info(info);
  }
}

fn void free_disk_list(DracDiskInfoList* list) {
  if (list != null) {
    draconis_raw::drac_free_disk_info_list(list);
  }
}

fn void free_display_list(DracDisplayInfoList* list) {
  if (list != null) {
    draconis_raw::drac_free_display_info_list(list);
  }
}

fn void free_network_interface(DracNetworkInterface* iface) {
  if (iface != null) {
    draconis_raw::drac_free_network_interface(iface);
  }
}

fn void free_network_interface_list(DracNetworkInterfaceList* list) {
  if (list != null) {
    draconis_raw::drac_free_network_interface_list(list);
  }
}

fn ulong get_uptime() {
  return draconis_raw::drac_get_uptime();
}

fn ErrorCode get_mem_info(CacheManager mgr, DracResourceUsage* usage) {
  return draconis_raw::drac_get_mem_info(mgr, usage);
}

fn ErrorCode get_cpu_cores(CacheManager mgr, DracCPUCores* cores) {
  return draconis_raw::drac_get_cpu_cores(mgr, cores);
}

fn ErrorCode get_operating_system(CacheManager mgr, DracOSInfo* info) {
  return draconis_raw::drac_get_operating_system(mgr, info);
}

fn char* get_desktop_environment(CacheManager mgr) {
  char* s = null;
  if (ok(draconis_raw::drac_get_desktop_environment(mgr, &s))) {
    return s;
  }
  return null;
}

fn char* get_window_manager(CacheManager mgr) {
  char* s = null;
  if (ok(draconis_raw::drac_get_window_manager(mgr, &s))) {
    return s;
  }
  return null;
}

fn char* get_shell(CacheManager mgr) {
  char* s = null;
  if (ok(draconis_raw::drac_get_shell(mgr, &s))) {
    return s;
  }
  return null;
}

fn char* get_host(CacheManager mgr) {
  char* s = null;
  if (ok(draconis_raw::drac_get_host(mgr, &s))) {
    return s;
  }
  return null;
}

fn char* get_cpu_model(CacheManager mgr) {
  char* s = null;
  if (ok(draconis_raw::drac_get_cpu_model(mgr, &s))) {
    return s;
  }
  return null;
}

fn char* get_gpu_model(CacheManager mgr) {
  char* s = null;
  if (ok(draconis_raw::drac_get_gpu_model(mgr, &s))) {
    return s;
  }
  return null;
}

fn char* get_kernel_version(CacheManager mgr) {
  char* s = null;
  if (ok(draconis_raw::drac_get_kernel_version(mgr, &s))) {
    return s;
  }
  return null;
}

fn ErrorCode get_disk_usage(CacheManager mgr, DracResourceUsage* usage) {
  return draconis_raw::drac_get_disk_usage(mgr, usage);
}

fn ErrorCode get_disks(CacheManager mgr, DracDiskInfoList* list) {
  return draconis_raw::drac_get_disks(mgr, list);
}

fn ErrorCode get_system_disk(CacheManager mgr, DracDiskInfo* info) {
  return draconis_raw::drac_get_system_disk(mgr, info);
}

fn ErrorCode get_outputs(CacheManager mgr, DracDisplayInfoList* list) {
  return draconis_raw::drac_get_outputs(mgr, list);
}

fn ErrorCode get_primary_output(CacheManager mgr, DracDisplayInfo* info) {
  return draconis_raw::drac_get_primary_output(mgr, info);
}

fn ErrorCode get_network_interfaces(CacheManager mgr, DracNetworkInterfaceList* list) {
  return draconis_raw::drac_get_network_interfaces(mgr, list);
}

fn ErrorCode get_primary_network_interface(CacheManager mgr, DracNetworkInterface* iface) {
  return draconis_raw::drac_get_primary_network_interface(mgr, iface);
}

fn ErrorCode get_battery_info(CacheManager mgr, DracBattery* battery) {
  return draconis_raw::drac_get_battery_info(mgr, battery);
}

// ============================== //
//  Plugin System                 //
// ============================== //

alias Plugin = DracPlugin*;

fn usz init_static_plugins() {
  return draconis_raw::drac_init_static_plugins();
}

fn void init_plugin_manager() {
  draconis_raw::drac_init_plugin_manager();
}

fn void shutdown_plugin_manager() {
  draconis_raw::drac_shutdown_plugin_manager();
}

fn void add_plugin_search_path(char* path) {
  if (path != null) {
    draconis_raw::drac_add_plugin_search_path(path);
  }
}

fn DracPluginInfoList discover_plugins() {
  return draconis_raw::drac_discover_plugins();
}

fn Plugin load_plugin(char* name) {
  if (name == null) return null;
  return draconis_raw::drac_load_plugin(name);
}

fn Plugin load_plugin_from_path(char* path) {
  if (path == null) return null;
  return draconis_raw::drac_load_plugin_from_path(path);
}

fn void unload_plugin(Plugin plugin) {
  if (plugin != null) {
    draconis_raw::drac_unload_plugin(plugin);
  }
}

fn ErrorCode plugin_initialize(Plugin plugin, CacheManager cache) {
  if (plugin == null || cache == null) return draconis_raw::DRAC_ERROR_INVALID_ARGUMENT;
  return draconis_raw::drac_plugin_initialize(plugin, cache);
}

fn bool plugin_is_enabled(Plugin plugin) {
  if (plugin == null) return false;
  return draconis_raw::drac_plugin_is_enabled(plugin);
}

fn bool plugin_is_ready(Plugin plugin) {
  if (plugin == null) return false;
  return draconis_raw::drac_plugin_is_ready(plugin);
}

fn ErrorCode plugin_collect_data(Plugin plugin, CacheManager cache) {
  if (plugin == null || cache == null) return draconis_raw::DRAC_ERROR_INVALID_ARGUMENT;
  return draconis_raw::drac_plugin_collect_data(plugin, cache);
}

fn char* plugin_get_json(Plugin plugin) {
  if (plugin == null) return null;
  return draconis_raw::drac_plugin_get_json(plugin);
}

fn DracPluginFieldList plugin_get_fields(Plugin plugin) {
  DracPluginFieldList empty = { null, 0 };
  if (plugin == null) return empty;
  return draconis_raw::drac_plugin_get_fields(plugin);
}

fn char* plugin_get_last_error(Plugin plugin) {
  if (plugin == null) return null;
  return draconis_raw::drac_plugin_get_last_error(plugin);
}

fn void free_plugin_info_list(DracPluginInfoList* list) {
  if (list != null) {
    draconis_raw::drac_free_plugin_info_list(list);
  }
}

fn void free_plugin_field_list(DracPluginFieldList* list) {
  if (list != null) {
    draconis_raw::drac_free_plugin_field_list(list);
  }
}
