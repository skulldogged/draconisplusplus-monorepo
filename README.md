# Draconis++ Monorepo

Cross-platform system information library with multi-language bindings.

## Features

- **Cross-platform**: Windows, macOS, Linux, FreeBSD, OpenBSD, NetBSD, Haiku, SerenityOS
- **Multi-language bindings**: C, C++, Rust, Python, Lua, C#, Kotlin, C3
- **Plugin system**: Static and dynamic plugin support
- **Zero-cost abstractions**: Efficient caching, minimal allocations

## Project Structure

```
├── core/           # Core C++ library
├── c-api/          # C API wrapper
├── bindings/       # Language bindings
│   ├── rust/       # Rust bindings
│   ├── python/     # Python bindings (nanobind)
│   ├── lua/        # Lua bindings (sol2)
│   ├── csharp/     # C# bindings
│   ├── kotlin/     # Kotlin/JNI bindings
│   └── c3/         # C3 bindings
└── subprojects/    # Meson subprojects (dependencies)
```

## Building

### Prerequisites

- Meson 1.1+
- Ninja
- C++26 compiler (MSVC 2022, Clang 18+, GCC 14+)

### Setup

```bash
meson setup build
meson compile -C build
```

### With Static Plugins

```bash
meson setup build -Dstatic_plugins=now_playing,weather   # or -Dstatic_plugins=all
meson compile -C build
```

## Language Bindings

### Rust

```toml
[dependencies]
draconis = { path = "bindings/rust" }
```

```rust
use draconis::{CacheManager, Plugin, init_static_plugins};

let mut cache = CacheManager::new();
let count = init_static_plugins();
let mut plugin = Plugin::new("NowPlayingPlugin")?;
plugin.initialize(&mut cache)?;
plugin.collect_data(&mut cache)?;
let fields = plugin.get_fields()?;
```

### Python

```python
import draconis

sys = draconis.SystemInfo()
print(f"Uptime: {sys.get_uptime()}s")
print(f"Memory: {sys.get_mem_info()}")
```

### Lua

```lua
local draconis = require("draconis")

local sys = draconis.SystemInfo()
print("Uptime:", sys:get_uptime())
```

### C#

```csharp
using Draconis;

using var client = new DraconisClient();
var uptime = client.GetUptimeSeconds();
var mem = client.GetMemoryUsage();
```

## Plugin System

Plugins are self-contained directories with a `plugin.json` manifest,
discovered automatically by the build — adding a plugin never requires
editing build files. Every plugin builds both ways without source changes:

- **Static plugins**: Compiled into the binary (`-Dstatic_plugins=name1,name2` or `all`), registered and loaded automatically at startup
- **Dynamic plugins**: Built as shared libraries (the default) and loaded at runtime from standard plugin directories

### Creating a Plugin

```bash
# Scaffold a working plugin in plugins/my_stats/ (or: just new-plugin my_stats)
python3 tools/plugin_helper.py new my_stats

# Build dynamically (default) or compile it into the binary
meson setup build && meson compile -C build
meson setup build -Dstatic_plugins=my_stats && meson compile -C build
```

User-made plugins can also live outside the repository — point the build at
the directory containing them:

```bash
meson setup build -Dplugin_dirs=$HOME/draconis-plugins -Dstatic_plugins=all
```

A plugin is just a class implementing one of the plugin interfaces plus the
`DRAC_PLUGIN()` registration macro:

```cpp
#include <Drac++/Core/Plugin.hpp>

class MyPlugin : public draconis::core::plugin::IInfoProviderPlugin {
  // ... implement interface
};

DRAC_PLUGIN(MyPlugin)
```

See [plugins/README.md](plugins/README.md) for the manifest schema and the
full authoring guide.

## License

MIT License - see [LICENSE](LICENSE)
