# Rust binding

The binding links the standalone `draconis_c` shared runtime. Meson owns its
transitive C++ and platform dependencies; Cargo no longer guesses archive paths.
By default the native build lives inside Cargo's target/profile-specific
`OUT_DIR`. `DRAC_MESON_NATIVE_FILE` selects a Meson native toolchain file.

Set `DRAC_NATIVE_BUILD_DIR` to an existing Meson build to consume it without
reconfiguring it. For cross compilation that build must target Cargo's `TARGET`;
automatic host builds are rejected. Make its `c-api` directory available to the
runtime loader (`PATH` on Windows, `LD_LIBRARY_PATH` on Linux,
`DYLD_LIBRARY_PATH` on macOS), or install the runtime normally before executing
the application. Ship the runtime with applications using this binding.
