//! High-level Rust types wrapping the C API

use std::ffi::CStr;

use crate::sys;

pub type DracErrorCode = i32;
pub type DracBatteryStatus = i32;

pub const DRAC_SUCCESS: DracErrorCode = 255;
pub const DRAC_ERROR_API_UNAVAILABLE: DracErrorCode = 0;
pub const DRAC_ERROR_CONFIGURATION_ERROR: DracErrorCode = 1;
pub const DRAC_ERROR_CORRUPTED_DATA: DracErrorCode = 2;
pub const DRAC_ERROR_INTERNAL_ERROR: DracErrorCode = 3;
pub const DRAC_ERROR_INVALID_ARGUMENT: DracErrorCode = 4;
pub const DRAC_ERROR_IO_ERROR: DracErrorCode = 5;
pub const DRAC_ERROR_NETWORK_ERROR: DracErrorCode = 6;
pub const DRAC_ERROR_NOT_FOUND: DracErrorCode = 7;
pub const DRAC_ERROR_NOT_SUPPORTED: DracErrorCode = 8;
pub const DRAC_ERROR_OTHER: DracErrorCode = 9;
pub const DRAC_ERROR_OUT_OF_MEMORY: DracErrorCode = 10;
pub const DRAC_ERROR_PARSE_ERROR: DracErrorCode = 11;
pub const DRAC_ERROR_PERMISSION_DENIED: DracErrorCode = 12;
pub const DRAC_ERROR_PERMISSION_REQUIRED: DracErrorCode = 13;
pub const DRAC_ERROR_PLATFORM_SPECIFIC: DracErrorCode = 14;
pub const DRAC_ERROR_RESOURCE_EXHAUSTED: DracErrorCode = 15;
pub const DRAC_ERROR_TIMEOUT: DracErrorCode = 16;
pub const DRAC_ERROR_UNAVAILABLE_FEATURE: DracErrorCode = 17;

pub const DRAC_BATTERY_UNKNOWN: DracBatteryStatus = 0;
pub const DRAC_BATTERY_CHARGING: DracBatteryStatus = 1;
pub const DRAC_BATTERY_DISCHARGING: DracBatteryStatus = 2;
pub const DRAC_BATTERY_FULL: DracBatteryStatus = 3;
pub const DRAC_BATTERY_NOT_PRESENT: DracBatteryStatus = 4;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum ErrorCode {
  ApiUnavailable,
  ConfigurationError,
  CorruptedData,
  InternalError,
  InvalidArgument,
  IoError,
  NetworkError,
  NotFound,
  NotSupported,
  Other,
  OutOfMemory,
  ParseError,
  PermissionDenied,
  PermissionRequired,
  PlatformSpecific,
  ResourceExhausted,
  Timeout,
  UnavailableFeature,
  Success,
}

impl From<DracErrorCode> for ErrorCode {
  fn from(code: DracErrorCode) -> Self {
    match code {
      DRAC_ERROR_API_UNAVAILABLE => ErrorCode::ApiUnavailable,
      DRAC_ERROR_CONFIGURATION_ERROR => ErrorCode::ConfigurationError,
      DRAC_ERROR_CORRUPTED_DATA => ErrorCode::CorruptedData,
      DRAC_ERROR_INTERNAL_ERROR => ErrorCode::InternalError,
      DRAC_ERROR_INVALID_ARGUMENT => ErrorCode::InvalidArgument,
      DRAC_ERROR_IO_ERROR => ErrorCode::IoError,
      DRAC_ERROR_NETWORK_ERROR => ErrorCode::NetworkError,
      DRAC_ERROR_NOT_FOUND => ErrorCode::NotFound,
      DRAC_ERROR_NOT_SUPPORTED => ErrorCode::NotSupported,
      DRAC_ERROR_OTHER => ErrorCode::Other,
      DRAC_ERROR_OUT_OF_MEMORY => ErrorCode::OutOfMemory,
      DRAC_ERROR_PARSE_ERROR => ErrorCode::ParseError,
      DRAC_ERROR_PERMISSION_DENIED => ErrorCode::PermissionDenied,
      DRAC_ERROR_PERMISSION_REQUIRED => ErrorCode::PermissionRequired,
      DRAC_ERROR_PLATFORM_SPECIFIC => ErrorCode::PlatformSpecific,
      DRAC_ERROR_RESOURCE_EXHAUSTED => ErrorCode::ResourceExhausted,
      DRAC_ERROR_TIMEOUT => ErrorCode::Timeout,
      DRAC_ERROR_UNAVAILABLE_FEATURE => ErrorCode::UnavailableFeature,
      DRAC_SUCCESS => ErrorCode::Success,
      _ => ErrorCode::Other,
    }
  }
}

pub type Result<T> = std::result::Result<T, ErrorCode>;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BatteryStatus {
  Unknown,
  Charging,
  Discharging,
  Full,
  NotPresent,
}

impl From<DracBatteryStatus> for BatteryStatus {
  fn from(status: DracBatteryStatus) -> Self {
    match status {
      DRAC_BATTERY_UNKNOWN => BatteryStatus::Unknown,
      DRAC_BATTERY_CHARGING => BatteryStatus::Charging,
      DRAC_BATTERY_DISCHARGING => BatteryStatus::Discharging,
      DRAC_BATTERY_FULL => BatteryStatus::Full,
      DRAC_BATTERY_NOT_PRESENT => BatteryStatus::NotPresent,
      _ => BatteryStatus::Unknown,
    }
  }
}

#[derive(Debug, Clone, Copy)]
pub struct ResourceUsage {
  pub used_bytes:  u64,
  pub total_bytes: u64,
}

#[derive(Debug, Clone, Copy)]
pub struct CPUCores {
  pub physical: usize,
  pub logical:  usize,
}

#[derive(Debug, Clone)]
pub struct OSInfo {
  pub name:    String,
  pub version: String,
  pub id:      String,
}

#[derive(Debug, Clone)]
pub struct DiskInfo {
  pub name:            String,
  pub mount_point:     String,
  pub filesystem:      String,
  pub drive_type:      String,
  pub total_bytes:     u64,
  pub used_bytes:      u64,
  pub is_system_drive: bool,
}

#[derive(Debug, Clone)]
pub struct DisplayInfo {
  pub id:           u64,
  pub width:        u64,
  pub height:       u64,
  pub refresh_rate: f64,
  pub is_primary:   bool,
}

#[derive(Debug, Clone)]
pub struct NetworkInterface {
  pub name:         String,
  pub ipv4_address: Option<String>,
  pub ipv6_address: Option<String>,
  pub mac_address:  Option<String>,
  pub is_up:        bool,
  pub is_loopback:  bool,
}

#[derive(Debug, Clone, Copy)]
pub struct Battery {
  pub status:              BatteryStatus,
  pub percentage:          Option<u8>,
  pub time_remaining_secs: Option<i64>,
}

pub struct CacheManager {
  handle: *mut sys::DracCacheManager,
}

impl CacheManager {
  pub fn new() -> Self {
    let handle = unsafe { sys::DracCreateCacheManager() };
    assert!(!handle.is_null(), "Failed to create cache manager");
    Self { handle }
  }
}

impl Default for CacheManager {
  fn default() -> Self {
    Self::new()
  }
}

impl Drop for CacheManager {
  fn drop(&mut self) {
    unsafe {
      sys::DracDestroyCacheManager(self.handle);
    }
  }
}

pub fn get_uptime() -> u64 {
  unsafe { sys::DracGetUptime() }
}

pub fn get_mem_info(cache: &mut CacheManager) -> Result<ResourceUsage> {
  let mut usage = sys::DracResourceUsage {
    usedBytes:  0,
    totalBytes: 0,
  };

  let result = unsafe { sys::DracGetMemInfo(cache.handle, &mut usage) };

  if result == DRAC_SUCCESS {
    Ok(ResourceUsage {
      used_bytes:  usage.usedBytes,
      total_bytes: usage.totalBytes,
    })
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_cpu_cores(cache: &mut CacheManager) -> Result<CPUCores> {
  let mut cores = sys::DracCPUCores {
    physical: 0,
    logical:  0,
  };

  let result = unsafe { sys::DracGetCpuCores(cache.handle, &mut cores) };

  if result == DRAC_SUCCESS {
    Ok(CPUCores {
      physical: cores.physical,
      logical:  cores.logical,
    })
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_operating_system(cache: &mut CacheManager) -> Result<OSInfo> {
  let mut info = sys::DracOSInfo {
    name:    std::ptr::null_mut(),
    version: std::ptr::null_mut(),
    id:      std::ptr::null_mut(),
  };

  let result = unsafe { sys::DracGetOperatingSystem(cache.handle, &mut info) };

  if result == DRAC_SUCCESS {
    let name = if info.name.is_null() {
      String::new()
    } else {
      unsafe { CStr::from_ptr(info.name) }
        .to_string_lossy()
        .into_owned()
    };
    let version = if info.version.is_null() {
      String::new()
    } else {
      unsafe { CStr::from_ptr(info.version) }
        .to_string_lossy()
        .into_owned()
    };
    let id = if info.id.is_null() {
      String::new()
    } else {
      unsafe { CStr::from_ptr(info.id) }
        .to_string_lossy()
        .into_owned()
    };

    unsafe { sys::DracFreeOSInfo(&mut info) };

    Ok(OSInfo {
      name,
      version,
      id,
    })
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_battery_info(cache: &mut CacheManager) -> Result<Battery> {
  let mut battery = sys::DracBattery {
    status:            DRAC_BATTERY_UNKNOWN,
    percentage:        255,
    timeRemainingSecs: -1,
  };

  let result = unsafe { sys::DracGetBatteryInfo(cache.handle, &mut battery) };

  if result == DRAC_SUCCESS {
    Ok(Battery {
      status:              BatteryStatus::from(battery.status),
      percentage:          if battery.percentage == 255 {
        None
      } else {
        Some(battery.percentage)
      },
      time_remaining_secs: if battery.timeRemainingSecs < 0 {
        None
      } else {
        Some(battery.timeRemainingSecs)
      },
    })
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_cpu_model(cache: &mut CacheManager) -> Result<String> {
  let mut ptr = std::ptr::null_mut();
  let result = unsafe { sys::DracGetCPUModel(cache.handle, &mut ptr) };

  if result == DRAC_SUCCESS && !ptr.is_null() {
    let s = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
    unsafe { sys::DracFreeString(ptr) };
    Ok(s)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_gpu_model(cache: &mut CacheManager) -> Result<String> {
  let mut ptr = std::ptr::null_mut();
  let result = unsafe { sys::DracGetGPUModel(cache.handle, &mut ptr) };

  if result == DRAC_SUCCESS && !ptr.is_null() {
    let s = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
    unsafe { sys::DracFreeString(ptr) };
    Ok(s)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_desktop_environment(cache: &mut CacheManager) -> Result<String> {
  let mut ptr = std::ptr::null_mut();
  let result = unsafe { sys::DracGetDesktopEnvironment(cache.handle, &mut ptr) };

  if result == DRAC_SUCCESS && !ptr.is_null() {
    let s = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
    unsafe { sys::DracFreeString(ptr) };
    Ok(s)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_window_manager(cache: &mut CacheManager) -> Result<String> {
  let mut ptr = std::ptr::null_mut();
  let result = unsafe { sys::DracGetWindowManager(cache.handle, &mut ptr) };

  if result == DRAC_SUCCESS && !ptr.is_null() {
    let s = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
    unsafe { sys::DracFreeString(ptr) };
    Ok(s)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_shell(cache: &mut CacheManager) -> Result<String> {
  let mut ptr = std::ptr::null_mut();
  let result = unsafe { sys::DracGetShell(cache.handle, &mut ptr) };

  if result == DRAC_SUCCESS && !ptr.is_null() {
    let s = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
    unsafe { sys::DracFreeString(ptr) };
    Ok(s)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_host(cache: &mut CacheManager) -> Result<String> {
  let mut ptr = std::ptr::null_mut();
  let result = unsafe { sys::DracGetHost(cache.handle, &mut ptr) };

  if result == DRAC_SUCCESS && !ptr.is_null() {
    let s = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
    unsafe { sys::DracFreeString(ptr) };
    Ok(s)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_kernel_version(cache: &mut CacheManager) -> Result<String> {
  let mut ptr = std::ptr::null_mut();
  let result = unsafe { sys::DracGetKernelVersion(cache.handle, &mut ptr) };

  if result == DRAC_SUCCESS && !ptr.is_null() {
    let s = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
    unsafe { sys::DracFreeString(ptr) };
    Ok(s)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_disk_usage(cache: &mut CacheManager) -> Result<ResourceUsage> {
  let mut usage = sys::DracResourceUsage {
    usedBytes:  0,
    totalBytes: 0,
  };

  let result = unsafe { sys::DracGetDiskUsage(cache.handle, &mut usage) };

  if result == DRAC_SUCCESS {
    Ok(ResourceUsage {
      used_bytes:  usage.usedBytes,
      total_bytes: usage.totalBytes,
    })
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_disks(cache: &mut CacheManager) -> Result<Vec<DiskInfo>> {
  let mut list = sys::DracDiskInfoList {
    items: std::ptr::null_mut(),
    count: 0,
  };

  let result = unsafe { sys::DracGetDisks(cache.handle, &mut list) };

  if result == DRAC_SUCCESS {
    let mut disks = Vec::with_capacity(list.count);

    for i in 0..list.count {
      let disk = unsafe { &*list.items.add(i) };
      disks.push(DiskInfo {
        name:            if disk.name.is_null() {
          String::new()
        } else {
          unsafe { CStr::from_ptr(disk.name) }
            .to_string_lossy()
            .into_owned()
        },
        mount_point:     if disk.mountPoint.is_null() {
          String::new()
        } else {
          unsafe { CStr::from_ptr(disk.mountPoint) }
            .to_string_lossy()
            .into_owned()
        },
        filesystem:      if disk.filesystem.is_null() {
          String::new()
        } else {
          unsafe { CStr::from_ptr(disk.filesystem) }
            .to_string_lossy()
            .into_owned()
        },
        drive_type:      if disk.driveType.is_null() {
          String::new()
        } else {
          unsafe { CStr::from_ptr(disk.driveType) }
            .to_string_lossy()
            .into_owned()
        },
        total_bytes:     disk.totalBytes,
        used_bytes:      disk.usedBytes,
        is_system_drive: disk.isSystemDrive,
      });
    }

    unsafe { sys::DracFreeDiskInfoList(&mut list) };
    Ok(disks)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_system_disk(cache: &mut CacheManager) -> Result<DiskInfo> {
  let mut disk = sys::DracDiskInfo {
    name:          std::ptr::null_mut(),
    mountPoint:    std::ptr::null_mut(),
    filesystem:    std::ptr::null_mut(),
    driveType:     std::ptr::null_mut(),
    totalBytes:    0,
    usedBytes:     0,
    isSystemDrive: false,
  };

  let result = unsafe { sys::DracGetSystemDisk(cache.handle, &mut disk) };

  if result == DRAC_SUCCESS {
    let info = DiskInfo {
      name:            if disk.name.is_null() {
        String::new()
      } else {
        unsafe { CStr::from_ptr(disk.name) }
          .to_string_lossy()
          .into_owned()
      },
      mount_point:     if disk.mountPoint.is_null() {
        String::new()
      } else {
        unsafe { CStr::from_ptr(disk.mountPoint) }
          .to_string_lossy()
          .into_owned()
      },
      filesystem:      if disk.filesystem.is_null() {
        String::new()
      } else {
        unsafe { CStr::from_ptr(disk.filesystem) }
          .to_string_lossy()
          .into_owned()
      },
      drive_type:      if disk.driveType.is_null() {
        String::new()
      } else {
        unsafe { CStr::from_ptr(disk.driveType) }
          .to_string_lossy()
          .into_owned()
      },
      total_bytes:     disk.totalBytes,
      used_bytes:      disk.usedBytes,
      is_system_drive: disk.isSystemDrive,
    };

    unsafe { sys::DracFreeDiskInfo(&mut disk) };
    Ok(info)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_outputs(cache: &mut CacheManager) -> Result<Vec<DisplayInfo>> {
  let mut list = sys::DracDisplayInfoList {
    items: std::ptr::null_mut(),
    count: 0,
  };

  let result = unsafe { sys::DracGetOutputs(cache.handle, &mut list) };

  if result == DRAC_SUCCESS {
    let mut displays = Vec::with_capacity(list.count);

    for i in 0..list.count {
      let display = unsafe { &*list.items.add(i) };
      displays.push(DisplayInfo {
        id:           display.id,
        width:        display.width,
        height:       display.height,
        refresh_rate: display.refreshRate,
        is_primary:   display.isPrimary,
      });
    }

    unsafe { sys::DracFreeDisplayInfoList(&mut list) };
    Ok(displays)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_primary_output(cache: &mut CacheManager) -> Result<DisplayInfo> {
  let mut display = sys::DracDisplayInfo {
    id:          0,
    width:       0,
    height:      0,
    refreshRate: 0.0,
    isPrimary:   false,
  };

  let result = unsafe { sys::DracGetPrimaryOutput(cache.handle, &mut display) };

  if result == DRAC_SUCCESS {
    Ok(DisplayInfo {
      id:           display.id,
      width:        display.width,
      height:       display.height,
      refresh_rate: display.refreshRate,
      is_primary:   display.isPrimary,
    })
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_network_interfaces(cache: &mut CacheManager) -> Result<Vec<NetworkInterface>> {
  let mut list = sys::DracNetworkInterfaceList {
    items: std::ptr::null_mut(),
    count: 0,
  };

  let result = unsafe { sys::DracGetNetworkInterfaces(cache.handle, &mut list) };

  if result == DRAC_SUCCESS {
    let mut interfaces = Vec::with_capacity(list.count);

    for i in 0..list.count {
      let iface = unsafe { &*list.items.add(i) };
      interfaces.push(NetworkInterface {
        name:         if iface.name.is_null() {
          String::new()
        } else {
          unsafe { CStr::from_ptr(iface.name) }
            .to_string_lossy()
            .into_owned()
        },
        ipv4_address: if iface.ipv4Address.is_null() {
          None
        } else {
          Some(
            unsafe { CStr::from_ptr(iface.ipv4Address) }
              .to_string_lossy()
              .into_owned(),
          )
        },
        ipv6_address: if iface.ipv6Address.is_null() {
          None
        } else {
          Some(
            unsafe { CStr::from_ptr(iface.ipv6Address) }
              .to_string_lossy()
              .into_owned(),
          )
        },
        mac_address:  if iface.macAddress.is_null() {
          None
        } else {
          Some(
            unsafe { CStr::from_ptr(iface.macAddress) }
              .to_string_lossy()
              .into_owned(),
          )
        },
        is_up:        iface.isUp,
        is_loopback:  iface.isLoopback,
      });
    }

    unsafe { sys::DracFreeNetworkInterfaceList(&mut list) };
    Ok(interfaces)
  } else {
    Err(ErrorCode::from(result))
  }
}

pub fn get_primary_network_interface(cache: &mut CacheManager) -> Result<NetworkInterface> {
  let mut iface = sys::DracNetworkInterface {
    name:        std::ptr::null_mut(),
    ipv4Address: std::ptr::null_mut(),
    ipv6Address: std::ptr::null_mut(),
    macAddress:  std::ptr::null_mut(),
    isUp:        false,
    isLoopback:  false,
  };

  let result = unsafe { sys::DracGetPrimaryNetworkInterface(cache.handle, &mut iface) };

  if result == DRAC_SUCCESS {
    let info = NetworkInterface {
      name:         if iface.name.is_null() {
        String::new()
      } else {
        unsafe { CStr::from_ptr(iface.name) }
          .to_string_lossy()
          .into_owned()
      },
      ipv4_address: if iface.ipv4Address.is_null() {
        None
      } else {
        Some(
          unsafe { CStr::from_ptr(iface.ipv4Address) }
            .to_string_lossy()
            .into_owned(),
        )
      },
      ipv6_address: if iface.ipv6Address.is_null() {
        None
      } else {
        Some(
          unsafe { CStr::from_ptr(iface.ipv6Address) }
            .to_string_lossy()
            .into_owned(),
        )
      },
      mac_address:  if iface.macAddress.is_null() {
        None
      } else {
        Some(
          unsafe { CStr::from_ptr(iface.macAddress) }
            .to_string_lossy()
            .into_owned(),
        )
      },
      is_up:        iface.isUp,
      is_loopback:  iface.isLoopback,
    };

    unsafe { sys::DracFreeNetworkInterface(&mut iface) };
    Ok(info)
  } else {
    Err(ErrorCode::from(result))
  }
}

// ============================== //
//  Plugin System                 //
// ============================== //

#[derive(Debug, Clone)]
pub struct PluginInfo {
  pub name:        String,
  pub version:     String,
  pub author:      String,
  pub description: String,
}

pub struct Plugin {
  handle: *mut sys::DracPlugin,
}

impl Plugin {
  pub fn new(plugin_name: &str) -> Result<Self> {
    let c_name = match std::ffi::CString::new(plugin_name) {
      Ok(s) => s,
      Err(_) => return Err(ErrorCode::InvalidArgument),
    };
    let handle = unsafe { sys::DracLoadPlugin(c_name.as_ptr()) };

    if handle.is_null() {
      Err(ErrorCode::NotFound)
    } else {
      Ok(Self { handle })
    }
  }

  pub fn from_path(path: &str) -> Result<Self> {
    let c_path = match std::ffi::CString::new(path) {
      Ok(s) => s,
      Err(_) => return Err(ErrorCode::InvalidArgument),
    };
    let handle = unsafe { sys::DracLoadPluginFromPath(c_path.as_ptr()) };

    if handle.is_null() {
      Err(ErrorCode::NotFound)
    } else {
      Ok(Self { handle })
    }
  }

  pub fn initialize(&mut self, cache: &mut CacheManager) -> Result<()> {
    let result = unsafe { sys::DracPluginInitialize(self.handle, cache.handle) };

    if result == DRAC_SUCCESS {
      Ok(())
    } else {
      Err(ErrorCode::from(result))
    }
  }

  /// Set plugin configuration from a TOML string.
  /// Must be called before `initialize()` for the config to take effect.
  ///
  /// # Example
  /// ```ignore
  /// let toml_config = r#"
  /// enabled = true
  /// provider = "openmeteo"
  /// units = "metric"
  /// [coords]
  /// lat = 40.7128
  /// lon = -74.0060
  /// "#;
  /// plugin.set_config(toml_config)?;
  /// plugin.initialize(&mut cache)?;
  /// ```
  pub fn set_config(&mut self, toml_config: &str) -> Result<()> {
    let c_config = std::ffi::CString::new(toml_config).map_err(|_| ErrorCode::InvalidArgument)?;
    let result = unsafe { sys::DracPluginSetConfig(self.handle, c_config.as_ptr()) };

    if result == DRAC_SUCCESS {
      Ok(())
    } else {
      Err(ErrorCode::from(result))
    }
  }

  pub fn is_enabled(&self) -> bool {
    unsafe { sys::DracPluginIsEnabled(self.handle) }
  }

  pub fn is_ready(&self) -> bool {
    unsafe { sys::DracPluginIsReady(self.handle) }
  }

  pub fn collect_data(&mut self, cache: &mut CacheManager) -> Result<()> {
    let result = unsafe { sys::DracPluginCollectData(self.handle, cache.handle) };

    if result == DRAC_SUCCESS {
      Ok(())
    } else {
      Err(ErrorCode::from(result))
    }
  }

  pub fn get_json(&self) -> Result<String> {
    let ptr = unsafe { sys::DracPluginGetJson(self.handle) };

    if ptr.is_null() {
      Err(ErrorCode::NotFound)
    } else {
      let s = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
      unsafe { sys::DracFreeString(ptr) };
      Ok(s)
    }
  }

  pub fn get_fields(&self) -> Result<std::collections::HashMap<String, String>> {
    let mut fields = unsafe { sys::DracPluginGetFields(self.handle) };

    let mut result = std::collections::HashMap::new();

    if fields.items.is_null() || fields.count == 0 {
      return Ok(result);
    }

    for i in 0..fields.count {
      let field = unsafe { &*fields.items.add(i) };
      let key = if field.key.is_null() {
        continue;
      } else {
        unsafe { CStr::from_ptr(field.key) }
          .to_string_lossy()
          .into_owned()
      };
      let value = if field.value.is_null() {
        String::new()
      } else {
        unsafe { CStr::from_ptr(field.value) }
          .to_string_lossy()
          .into_owned()
      };
      result.insert(key, value);
    }

    unsafe { sys::DracFreePluginFieldList(&mut fields) };

    Ok(result)
  }

  pub fn get_last_error(&self) -> Option<String> {
    let ptr = unsafe { sys::DracPluginGetLastError(self.handle) };

    if ptr.is_null() {
      None
    } else {
      let s = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
      unsafe { sys::DracFreeString(ptr) };
      Some(s)
    }
  }
}

impl Drop for Plugin {
  fn drop(&mut self) {
    unsafe {
      sys::DracUnloadPlugin(self.handle);
    }
  }
}

pub fn initialize_plugin_manager() {
  unsafe { sys::DracInitPluginManager() };
}

pub fn shutdown_plugin_manager() {
  unsafe { sys::DracShutdownPluginManager() };
}

pub fn add_plugin_search_path(path: &str) {
  if let Ok(c_path) = std::ffi::CString::new(path) {
    unsafe { sys::DracAddPluginSearchPath(c_path.as_ptr()) };
  }
}

pub fn discover_plugins() -> Result<Vec<PluginInfo>> {
  let mut list = unsafe { sys::DracDiscoverPlugins() };

  if list.items.is_null() || list.count == 0 {
    return Ok(Vec::new());
  }

  let mut result = Vec::with_capacity(list.count);

  for i in 0..list.count {
    let info = unsafe { &*list.items.add(i) };
    result.push(PluginInfo {
      name:        if info.name.is_null() {
        String::new()
      } else {
        unsafe { CStr::from_ptr(info.name) }
          .to_string_lossy()
          .into_owned()
      },
      version:     if info.version.is_null() {
        String::new()
      } else {
        unsafe { CStr::from_ptr(info.version) }
          .to_string_lossy()
          .into_owned()
      },
      author:      if info.author.is_null() {
        String::new()
      } else {
        unsafe { CStr::from_ptr(info.author) }
          .to_string_lossy()
          .into_owned()
      },
      description: if info.description.is_null() {
        String::new()
      } else {
        unsafe { CStr::from_ptr(info.description) }
          .to_string_lossy()
          .into_owned()
      },
    });
  }

  unsafe { sys::DracFreePluginInfoList(&mut list) };

  Ok(result)
}
