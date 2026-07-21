# Plugin authoring

A Draconis++ plugin is a self-contained directory with a `plugin.json` manifest
and one or more C++ sources. Official and third-party plugins live outside the
core repository and use the same build machinery.

Every plugin can be built dynamically (as a shared module loaded at runtime) or
statically (compiled into the Draconis++ binary) without source changes.

## Create a plugin

Choose an external directory that will contain your plugin checkouts, then run:

```bash
python3 tools/plugin_helper.py new my_stats --dir ~/draconis-plugins
```

This creates `~/draconis-plugins/my_stats/` with a working `plugin.json` and
`my_stats.cpp` implementing `IInfoProviderPlugin`.

Build it as a dynamic module:

```bash
meson setup build -Dplugin_dirs=$HOME/draconis-plugins
meson compile -C build my_stats
```

Or compile it into Draconis++:

```bash
meson setup build-static \
  -Dplugin_dirs=$HOME/draconis-plugins \
  -Dstatic_plugins=my_stats
meson compile -C build-static
```

`-Dplugin_dirs` is an array option, so it can receive multiple plugin roots.
Each immediate subdirectory containing a `plugin.json` is discovered as a
plugin. Use `-Dstatic_plugins=all` to statically link every discovered plugin.

## Manifest reference

The manifest is the source of truth for how a plugin is built:

```jsonc
{
  // Defaults to the directory name. <name>.cpp is included automatically
  // when that file exists.
  "name": "now_playing",

  // Class registered by DRAC_PLUGIN(); defaults to "name".
  "class": "NowPlayingPlugin",

  "description": "Provides currently playing media information",

  // "all" (the default), a platform string, or a list of Meson host system
  // names such as linux, darwin, windows, freebsd, or haiku.
  "platforms": ["all"],

  // macOS only: request ad-hoc signing of a binary containing this static
  // plugin. This is intended for plugins that access private frameworks.
  "codesign": true,

  // Extra sources beyond the implicit <name>.cpp. Entries may be filtered by
  // host platform.
  "sources": [
    "extra_helpers.cpp",
    { "file": "now_playing_macos.mm", "platforms": ["darwin"] }
  ],

  // Dependencies are resolved with Meson's dependency(). Optional fields are
  // include_type ("preserve" by default), static (false by default), and
  // platforms (all platforms by default).
  "deps": [
    {
      "name": "dbus-1",
      "include_type": "system",
      "platforms": ["linux", "freebsd"]
    }
  ]
}
```

For a dynamic build, a plugin with a missing dependency is skipped with a
warning. A missing dependency requested by a static build is an error.

### Nix plugin package metadata

Plugin-root derivations should expose the following passthru metadata:

```nix
passthru = {
  pluginNames = ["my_stats"];
  pluginBuildInputsByName.my_stats = [pkgs.someDependency];
  pluginBuildInputs = [pkgs.someDependency];
};
```

The Home Manager module uses `pluginBuildInputsByName` to add dependencies only
for plugins selected by `staticPlugins`. `pluginBuildInputs` is retained as a
compatibility fallback for older plugin packages. Set `pluginMode = "static"`
to compile every plugin contained in the supplied roots without repeating the
names in `staticPlugins`.

## Implement a plugin

Plugins implement an interface from `<Drac++/Core/Plugin.hpp>`:

- `IInfoProviderPlugin` contributes data such as weather, media, or service
  statistics.
- `IOutputFormatPlugin` contributes an output format such as JSON or YAML.

Register the implementation with `DRAC_PLUGIN()`:

```cpp
#include <Drac++/Core/Plugin.hpp>

class MyStatsPlugin final
    : public draconis::core::plugin::IInfoProviderPlugin {
  // Implement the interface.
};

DRAC_PLUGIN(MyStatsPlugin)
```

In a dynamic build, `DRAC_PLUGIN()` emits the entry points used by the runtime
loader. In a static build, `DRAC_STATIC_PLUGIN_BUILD` changes it to emit a
registration function invoked by `DracInitStaticPlugins()` during startup.

## Dynamic plugin discovery

At runtime, Draconis++ scans these locations in order:

1. Directories in `DRAC_PLUGIN_PATH` (colon-separated on Unix and
   semicolon-separated on Windows).
2. On Unix: `/usr/local/lib/draconis++/plugins`,
   `/usr/lib/draconis++/plugins`, `~/.local/lib/draconis++/plugins`, and
   `./plugins`.
3. On Windows: `%LOCALAPPDATA%\draconis++\plugins`,
   `%APPDATA%\draconis++\plugins`, and `.\plugins`.

`meson install` places dynamic modules in `<libdir>/draconis++/plugins`. To test
without installing, point `DRAC_PLUGIN_PATH` at the directory containing the
built module.

If the same provider is available both statically and dynamically, the static
version wins.

## Plugin configuration

Each plugin owns its runtime configuration. The default location is
`~/.config/draconis++/plugins/<name>.toml` on Unix and the corresponding
`%LOCALAPPDATA%` location on Windows. A plugin receives its configuration
directory through `PluginContext` during `initialize()`.

## Build pipeline

[`tools/plugin_helper.py`](../tools/plugin_helper.py) handles the manifest in
three stages:

1. `discover` finds plugin directories in the roots passed through
   `-Dplugin_dirs`.
2. `info` resolves platform-specific sources and dependencies.
3. `registry` generates `StaticPluginInit.cpp` for static builds.

The top-level `meson.build` creates dynamic `shared_module` targets. Static
plugin sources are compiled into the core library by
`core/src/Lib/meson.build`.
