//! Draconis - Cross-platform system information library
//!
//! This crate provides safe Rust bindings to the Draconis C library,
//! which provides system information across multiple platforms.
//!
//! # Static Plugins
//!
//! When using static plugins, call `init_static_plugins()` before loading any plugins:
//!
//! ```ignore
//! draconis::init_static_plugins();
//! let plugin = draconis::Plugin::new("NowPlayingPlugin").expect("Failed to load");
//! ```

mod sys;
mod types;

pub use types::*;

/// Initialize static plugins.
///
/// This MUST be called before `Plugin::new()` when using static plugins.
/// Returns the number of plugins registered.
///
/// On builds without static plugins, this is a no-op that returns 0.
#[must_use = "The returned count should be checked to verify plugins were registered"]
pub fn init_static_plugins() -> usize {
  unsafe { sys::DracInitStaticPlugins() }
}

#[cfg(test)]
mod tests {
  use super::*;

  #[test]
  fn test_cache_manager() {
    let cache = CacheManager::new();
    drop(cache);
  }

  #[test]
  fn test_uptime() {
    let uptime = get_uptime();
    assert!(uptime > 0);
  }

  #[test]
  fn test_memory_info() {
    let mut cache = CacheManager::new();
    let usage = get_mem_info(&mut cache).expect("Failed to get memory info");
    assert!(usage.total_bytes > 0);
  }

  #[test]
  fn test_cpu_cores() {
    let mut cache = CacheManager::new();
    let cores = get_cpu_cores(&mut cache).expect("Failed to get CPU cores");
    assert!(cores.logical > 0);
  }
}
