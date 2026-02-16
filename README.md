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
DRAC_STATIC_PLUGINS=now_playing meson setup build
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

Draconis++ supports both static and dynamic plugins:

- **Static plugins**: Compiled into the binary, registered at startup
- **Dynamic plugins**: Loaded from shared libraries at runtime

### Creating a Plugin

```cpp
#include <Drac++/Core/Plugin.hpp>

class MyPlugin : public draconis::core::plugin::IInfoProviderPlugin {
  // ... implement interface
};

DRAC_PLUGIN(MyPlugin)
```

## License

MIT License - see [LICENSE](LICENSE)
