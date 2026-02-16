use std::{
  env,
  path::{Path, PathBuf},
  process::Command,
};

fn main() {
  let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
  let out_dir = env::var("OUT_DIR").unwrap();
  let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_else(|_| "unknown".to_string());

  let monorepo_root = Path::new(&manifest_dir)
    .parent()
    .and_then(|p| p.parent())
    .expect("Failed to find monorepo root");

  let build_dir = monorepo_root.join("build");

  println!("cargo:rerun-if-env-changed=DRAC_PLUGINS");
  println!("cargo:rerun-if-env-changed=DRAC_STATIC_PLUGINS");
  println!("cargo:rerun-if-env-changed=DRAC_PACKAGECOUNT");
  println!("cargo:rerun-if-env-changed=DRAC_CACHING");
  println!("cargo:rerun-if-env-changed=DRAC_BUILD_TYPE");

  run_meson_build(&monorepo_root, &build_dir);

  generate_bindings(&monorepo_root, &out_dir);

  link_libraries(&build_dir);
  link_system_libs(&target_os);
}

fn run_meson_build(monorepo_root: &Path, build_dir: &Path) {
  let is_configured = build_dir.join("build.ninja").exists();

  let plugins = env::var("DRAC_PLUGINS").ok();
  let static_plugins = env::var("DRAC_STATIC_PLUGINS").ok();
  let packagecount = env::var("DRAC_PACKAGECOUNT").ok();
  let caching = env::var("DRAC_CACHING").ok();
  let build_type = env::var("DRAC_BUILD_TYPE").ok();

  let needs_reconfigure = !is_configured
    || plugins.is_some()
    || static_plugins.is_some()
    || packagecount.is_some()
    || caching.is_some()
    || build_type.is_some();

  if !is_configured {
    let mut args = vec![
      "setup".to_string(),
      build_dir.to_string_lossy().to_string(),
      monorepo_root.to_string_lossy().to_string(),
      "-Dbuild_cli=false".to_string(),
      "-Dbuild_tests=false".to_string(),
      "-Dbuild_examples=false".to_string(),
      "-Dbuild_rust=false".to_string(),
      "-Db_vscrt=md".to_string(),
    ];

    let bt = build_type.as_deref().unwrap_or("release");
    args.push(format!("--buildtype={}", bt));

    // If static plugins are specified, enable the plugin system
    if let Some(val) = &static_plugins {
      args.push("-Dplugins=enabled".to_string());
      args.push(format!("-Dstatic_plugins={}", val));
      args.push("-Dprecompiled_config=true".to_string());
    } else {
      args.push(format!(
        "-Dplugins={}",
        plugins.as_deref().unwrap_or("auto")
      ));
    }

    if let Some(val) = &packagecount {
      args.push(format!("-Dpackagecount={}", val));
    }

    if let Some(val) = &caching {
      args.push(format!("-Dcaching={}", val));
    }

    let status = Command::new("meson")
      .args(&args)
      .status()
      .expect("Failed to run meson setup. Is Meson installed?");

    if !status.success() {
      panic!("meson setup failed");
    }
  } else if needs_reconfigure {
    let mut args = vec![
      "configure".to_string(),
      build_dir.to_string_lossy().to_string(),
    ];

    if let Some(val) = &plugins {
      args.push(format!("-Dplugins={}", val));
    }

    if let Some(val) = &static_plugins {
      args.push(format!("-Dstatic_plugins={}", val));
    }

    if let Some(val) = &packagecount {
      args.push(format!("-Dpackagecount={}", val));
    }

    if let Some(val) = &caching {
      args.push(format!("-Dcaching={}", val));
    }

    if let Some(val) = &build_type {
      args.push(format!("--buildtype={}", val));
    }

    let status = Command::new("meson")
      .args(&args)
      .status()
      .expect("Failed to run meson configure");

    if !status.success() {
      panic!("meson configure failed");
    }
  }

  let status = Command::new("meson")
    .args(["compile", "-C", build_dir.to_str().unwrap()])
    .status()
    .expect("Failed to run meson compile");

  if !status.success() {
    panic!("meson compile failed");
  }
}

fn generate_bindings(monorepo_root: &Path, out_dir: &str) {
  let header_path = monorepo_root.join("c-api/include/draconis_c.h");

  let builder = bindgen::Builder::default()
    .header(header_path.to_string_lossy())
    .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
    .generate_block(true)
    .block_extern_crate(true)
    .default_enum_style(bindgen::EnumVariation::Consts)
    .allowlist_function("Drac.*")
    .allowlist_type("Drac.*");

  let bindings = builder.generate().expect("Unable to generate bindings");

  bindings
    .write_to_file(PathBuf::from(out_dir).join("bindings.rs"))
    .expect("Couldn't write bindings!");
}

fn link_libraries(build_dir: &Path) {
  println!(
    "cargo:rustc-link-search=native={}",
    build_dir.join("c-api").display()
  );
  println!(
    "cargo:rustc-link-search=native={}",
    build_dir.join("core/src/Lib").display()
  );

  let mimalloc_dir = build_dir.join("subprojects/mimalloc-3.1.5");
  let has_mimalloc = mimalloc_dir.exists();
  if has_mimalloc {
    println!("cargo:rustc-link-search=native={}", mimalloc_dir.display());
  }

  println!("cargo:rustc-link-lib=static=drac++");
  println!("cargo:rustc-link-lib=static=draconis_c");

  if has_mimalloc {
    println!("cargo:rustc-link-lib=static=mimalloc");
  }
}

fn link_system_libs(target_os: &str) {
  match target_os {
    "windows" => {
      for lib in &[
        "dwmapi", "setupapi", "dxgi", "dxguid", "ole32", "propsys", "iphlpapi", "ws2_32",
        "advapi32", "user32", "shell32", "psapi", "bcrypt",
      ] {
        println!("cargo:rustc-link-lib=dylib={}", lib);
      }
    }
    "macos" => {
      println!("cargo:rustc-link-lib=framework=CoreGraphics");
      println!("cargo:rustc-link-lib=framework=Foundation");
      println!("cargo:rustc-link-lib=framework=IOKit");
      println!("cargo:rustc-link-lib=framework=SystemConfiguration");
    }
    "linux" | "freebsd" | "netbsd" | "openbsd" => {
      println!("cargo:rustc-link-lib=dylib=dl");
    }
    _ => {}
  }
}
