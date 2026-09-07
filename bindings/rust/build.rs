use std::{
  env,
  path::{Path, PathBuf},
  process::Command,
};

fn main() {
  let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
  let out_dir = env::var("OUT_DIR").unwrap();

  let monorepo_root = Path::new(&manifest_dir)
    .parent()
    .and_then(|p| p.parent())
    .expect("Failed to find monorepo root");

  // Cargo isolates OUT_DIR by target/profile/configuration. Never mutate a
  // shared checkout build, including when several Cargo jobs run concurrently.
  let build_dir = PathBuf::from(&out_dir).join("native");
  if env::var("HOST").ok() != env::var("TARGET").ok()
    && env::var_os("DRAC_NATIVE_BUILD_DIR").is_none()
  {
    panic!("Cross compilation requires a separately built target SDK; set DRAC_NATIVE_BUILD_DIR to its Meson build directory");
  }
  println!("cargo:rerun-if-env-changed=DRAC_NATIVE_BUILD_DIR");
  println!("cargo:rerun-if-env-changed=DRAC_MESON_NATIVE_FILE");
  for path in ["core", "c-api", "tools", "meson.build", "meson.options"] {
    println!(
      "cargo:rerun-if-changed={}",
      monorepo_root.join(path).display()
    );
  }

  println!("cargo:rerun-if-env-changed=DRAC_PLUGINS");
  println!("cargo:rerun-if-env-changed=DRAC_PLUGIN_DIRS");
  println!("cargo:rerun-if-env-changed=DRAC_STATIC_PLUGINS");
  println!("cargo:rerun-if-env-changed=DRAC_PACKAGECOUNT");
  println!("cargo:rerun-if-env-changed=DRAC_CACHING");
  println!("cargo:rerun-if-env-changed=DRAC_BUILD_TYPE");

  let build_dir = if let Some(path) = env::var_os("DRAC_NATIVE_BUILD_DIR") {
    PathBuf::from(path)
  } else {
    run_meson_build(&monorepo_root, &build_dir);
    build_dir
  };

  generate_bindings(&monorepo_root, &out_dir);

  link_libraries(&build_dir);
}

fn run_meson_build(monorepo_root: &Path, build_dir: &Path) {
  let configured = build_dir.join("build.ninja").exists();
  let static_plugins = env::var("DRAC_STATIC_PLUGINS").unwrap_or_default();
  let plugins = if static_plugins.is_empty() {
    env::var("DRAC_PLUGINS").unwrap_or_else(|_| "auto".to_string())
  } else {
    "enabled".to_string()
  };
  let mut args = vec![
    "setup".to_string(),
    build_dir.display().to_string(),
    monorepo_root.display().to_string(),
    "-Dbuild_cli=false".to_string(),
    "-Dbuild_tests=false".to_string(),
    "-Dbuild_examples=false".to_string(),
    "-Dbuild_rust=false".to_string(),
    "-Db_vscrt=md".to_string(),
    "-Dprecompiled_config=false".to_string(),
    format!("-Dplugins={}", plugins),
    format!("-Dstatic_plugins={}", static_plugins),
    format!(
      "-Dplugin_dirs={}",
      env::var("DRAC_PLUGIN_DIRS").unwrap_or_default()
    ),
    format!(
      "-Dpackagecount={}",
      env::var("DRAC_PACKAGECOUNT").unwrap_or_else(|_| "auto".to_string())
    ),
    format!(
      "-Dcaching={}",
      env::var("DRAC_CACHING").unwrap_or_else(|_| "auto".to_string())
    ),
    format!(
      "--buildtype={}",
      env::var("DRAC_BUILD_TYPE").unwrap_or_else(|_| "release".to_string())
    ),
  ];
  // Reapply defaults as well as explicit overrides when Cargo reruns us, so
  // removing an environment override cannot leave a stale native configuration.
  if configured {
    args.push("--reconfigure".to_string());
  } else if let Ok(path) = env::var("DRAC_MESON_NATIVE_FILE") {
    args.extend(["--native-file".to_string(), path]);
  }
  assert!(
    Command::new("meson")
      .args(args)
      .status()
      .expect("Run Meson setup")
      .success(),
    "meson setup failed"
  );
  assert!(
    Command::new("meson")
      .args(["compile", "-C", build_dir.to_str().unwrap()])
      .status()
      .expect("Run Meson compile")
      .success(),
    "meson compile failed"
  );
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
  // Meson owns C++ runtime, curl and platform transitive dependencies. Link
  // only its standalone C runtime instead of guessing private archive paths.
  println!("cargo:rustc-link-lib=dylib=draconis_c");
}
