# ============================== #
#  Rust Bindings                 #
# ============================== #

# This meson.build provides integration with the Meson build system.
# It does NOT build the Rust crate - that's handled by Cargo via build.rs.
# Instead, it provides a dependency that can be consumed by other Meson targets.

rust_bindings_dir = meson.current_source_dir()

# Create a dependency that other Meson targets can use
# This links the C library and provides include paths
draconis_rust_dep = declare_dependency(
  dependencies: [draconis_c_dep, draconis_dep],
)

# Note: To build the Rust crate, run cargo from this directory:
#   cd bindings/rust && cargo build
#
# Or use environment variables to configure:
#   DRAC_STATIC_PLUGINS=now_playing,weather cargo build --release
