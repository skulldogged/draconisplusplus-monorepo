# Draconis++

Draconis++ is a fast, cross-platform system information tool and library written
in C++26. It provides a configurable command-line interface, reusable C++ and C
APIs, language bindings, and an external plugin system.

## Highlights

- Collects operating system, kernel, host, CPU, GPU, memory, disk, uptime,
  desktop, window manager, shell, and package information
- Runs on Windows, macOS, Linux, FreeBSD, OpenBSD, NetBSD, Haiku, and
  SerenityOS
- Supports compact templates, localized output, custom terminal logos, caching,
  diagnostics, and shell completion generation
- Exposes C++, C, Rust, Python, Lua, C#, Kotlin, and C3 interfaces
- Loads third-party data providers and output formats as dynamic modules or
  compiles them into the binary

## Build from source

You will need:

- Meson 1.1 or newer
- Ninja
- Python 3
- A compiler and standard library with the C++26 features used by the project

The recommended workflow uses [`just`](https://github.com/casey/just):

```bash
just setup
just build
just run
```

The equivalent Meson commands are:

```bash
meson setup build
meson compile -C build
./build/core/src/CLI/draconis++
```

On Windows, the executable is
`build\core\src\CLI\draconis++.exe`.

Useful development commands:

```bash
just test                  # run the test suite
just test-one "Types"      # run one Meson test
just format                # format C and C++ sources
just lint                  # run clang-tidy against the configured build
```

Meson builds the CLI, C++ library, C API, and tests by default. Optional targets
can be enabled at setup time:

```bash
meson setup build \
  -Dbuild_examples=true \
  -Dbuild_python=true \
  -Dbuild_rust=true
```

Run `meson configure build` to see every available option. Reconfigure an
existing build with `just configure -Doption=value` or
`meson setup build --reconfigure -Doption=value`.

## Using the CLI

Run `draconis++ --help` for the complete command reference. A few useful modes
are:

```bash
draconis++ --doctor
draconis++ --benchmark
draconis++ --compact '{host} | {cpu} | {ram}'
draconis++ --generate-completions zsh
draconis++ --show-config-path
```

On its first run, Draconis++ creates a TOML configuration file in the normal
per-user configuration directory. `--show-config-path` prints the location in
use. Builds intended for immutable or declarative systems can instead enable
`-Dprecompiled_config=true` and provide a `config.hpp` based on
[`config.example.hpp`](config.example.hpp).

## Plugins

Plugins live outside this repository. A plugin is a self-contained directory
with a `plugin.json` manifest and one or more C++ source files. Pass the parent
directory containing plugin checkouts through `-Dplugin_dirs`:

```bash
# Create ~/draconis-plugins/my_stats
python3 tools/plugin_helper.py new my_stats --dir "$HOME/draconis-plugins"

# Build all discovered plugins as runtime-loadable modules
meson setup build -Dplugin_dirs="$HOME/draconis-plugins"
meson compile -C build

# Or compile selected plugins into Draconis++
meson setup build-static \
  -Dplugin_dirs="$HOME/draconis-plugins" \
  -Dstatic_plugins=my_stats
meson compile -C build-static
```

Use `-Dstatic_plugins=all` to compile every discovered plugin statically.
Dynamic modules are installed under `<libdir>/draconis++/plugins` and can also
be found at runtime through `DRAC_PLUGIN_PATH`.

See the [plugin authoring guide](docs/plugins.md) for the manifest schema,
plugin interfaces, runtime search paths, configuration, and build behavior.

## Language bindings

All non-C++ bindings use the stable C API in
[`c-api/include/draconis_c.h`](c-api/include/draconis_c.h). Enable the binding
you need with its Meson option:

| Binding | Meson option |
| --- | --- |
| Rust | `-Dbuild_rust=true` |
| Python | `-Dbuild_python=true` |
| Lua | `-Dbuild_lua=true` |
| C# | `-Dbuild_csharp=true` |
| Kotlin | `-Dbuild_kotlin=true` |
| C3 | `-Dbuild_c3=true` |

The C++ headers and C API header are installed by `meson install -C build`.

## Nix and Home Manager

The repository includes a Nix package and Home Manager module. Plugin source
roots can be supplied with `pluginDirs`; packaged plugin collections can be
supplied with `pluginPackages`. The module also supports static plugin selection
and runtime auto-loading through `staticPlugins` and `pluginAutoLoad`.

## License

Draconis++ is available under the [MIT License](LICENSE).
