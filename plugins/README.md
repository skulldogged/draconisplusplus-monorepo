# Draconis++ Plugins

A plugin is a **self-contained directory** with a `plugin.json` manifest and
one or more C++ sources. The build system discovers plugins automatically —
adding a plugin never requires editing any build files.

Plugins are discovered from:

1. This directory (`plugins/`) — every subdirectory containing a `plugin.json`
2. Any extra directories passed at setup time via `-Dplugin_dirs=/path/to/my-plugins`

Both bundled and user-made plugins go through exactly the same machinery
(`tools/plugin_helper.py`), and every plugin can be built **dynamically**
(a shared library loaded at runtime) or **statically** (compiled into the
binary) with no source changes.

## Quick Start: Creating a Plugin

Scaffold a new plugin (or use `just new-plugin my_stats`):

```bash
python3 tools/plugin_helper.py new my_stats
```

This creates `plugins/my_stats/` with a working `plugin.json` and
`my_stats.cpp` implementing `IInfoProviderPlugin`. Then build it:

```bash
# Dynamic plugin (default) - produces build/plugins/my_stats.so
meson setup build && meson compile -C build

# Static plugin - compiled into the draconis++ binary
meson setup build -Dstatic_plugins=my_stats && meson compile -C build
```

To keep your plugin outside this repository, put the plugin directory
anywhere and point the build at its **parent** directory:

```bash
python3 tools/plugin_helper.py new my_stats --dir ~/draconis-plugins
meson setup build -Dplugin_dirs=$HOME/draconis-plugins -Dstatic_plugins=my_stats
```

`-Dstatic_plugins=all` statically links every discovered plugin.

## The Manifest (`plugin.json`)

The manifest is the single source of truth for how a plugin is built:

```jsonc
{
  // Plugin name; defaults to the directory name. <name>.cpp is the implicit
  // main source if it exists.
  "name": "now_playing",

  // C++ class registered via DRAC_PLUGIN(); defaults to "name".
  "class": "NowPlayingPlugin",

  "description": "Provides currently playing media information",

  // Platforms this plugin supports: "all" (default) or a list of Meson
  // host_machine.system() values: linux, darwin, windows, freebsd, netbsd,
  // openbsd, dragonfly, haiku, serenity.
  "platforms": ["all"],

  // macOS only: the installed binary needs ad-hoc codesigning (for private
  // framework access). Only meaningful for static plugins.
  "codesign": true,

  // Additional sources beyond the implicit <name>.cpp. Entries are plain
  // strings or objects with a platform filter.
  "sources": [
    "extra_helpers.cpp",
    { "file": "now_playing_macos.mm", "platforms": ["darwin"] }
  ],

  // External dependencies, resolved with Meson's dependency() (pkg-config,
  // CMake, etc.). Optional fields: include_type ("preserve" default),
  // static (false default), platforms (everywhere by default).
  "deps": [
    { "name": "dbus-1", "include_type": "system", "platforms": ["linux", "freebsd"] }
  ]
}
```

For dynamic builds, a plugin whose dependency is missing on the host is
skipped with a warning; for static builds a missing dependency is an error.

## Plugin Code

A plugin implements one of the interfaces from `<Drac++/Core/Plugin.hpp>`:

- `IInfoProviderPlugin` — contributes data (weather, media, docker stats, ...)
- `IOutputFormatPlugin` — contributes output formats (JSON, YAML, ...)

and registers its class with the `DRAC_PLUGIN()` macro:

```cpp
#include <Drac++/Core/Plugin.hpp>

class MyStatsPlugin final : public draconis::core::plugin::IInfoProviderPlugin {
  // ... implement the interface
};

DRAC_PLUGIN(MyStatsPlugin)
```

`DRAC_PLUGIN()` adapts automatically: in dynamic builds it emits the
`CreatePlugin`/`DestroyPlugin` entry points used by `dlopen`; in static builds
(`DRAC_STATIC_PLUGIN_BUILD`) it emits a registration function that
`DracInitStaticPlugins()` — called by `PluginManager::initialize()` — invokes
at startup.

## How Plugins Are Loaded at Runtime

- **Static plugins** are registered and loaded automatically at startup.
- **Dynamic plugins** are discovered by scanning, in order:
  - any directories in the `DRAC_PLUGIN_PATH` environment variable
    (colon-separated on Unix, semicolon-separated on Windows)
  - `/usr/local/lib/draconis++/plugins`, `/usr/lib/draconis++/plugins`,
    `~/.local/lib/draconis++/plugins`, and `./plugins` (Unix)
  - `%LOCALAPPDATA%\draconis++\plugins`, `%APPDATA%\draconis++\plugins`,
    and `.\plugins` (Windows)

  `meson install` installs in-tree dynamic plugins to
  `<libdir>/draconis++/plugins`. To test without installing, copy the built
  module (e.g. `build/plugins/my_stats.so`) into one of the directories above.

If a plugin is available both statically and dynamically, the static version
wins (matched by provider ID).

Per-plugin runtime configuration lives in
`~/.config/draconis++/plugins/<name>.toml` (XDG paths on Unix,
`%LOCALAPPDATA%` on Windows); each plugin reads its own config during
`initialize()` via the `PluginContext` it receives.

## Build Pipeline Internals

```
plugin.json ──┐
              ├── tools/plugin_helper.py discover  → finds plugin directories
              ├── tools/plugin_helper.py info      → sources/deps for the host platform
              └── tools/plugin_helper.py registry  → generated StaticPluginInit.cpp
```

- Dynamic builds: `plugins/meson.build` builds each discovered plugin as a
  `shared_module`.
- Static builds: `core/src/Lib/meson.build` compiles plugin sources directly
  into the core library and generates the registration translation unit.
