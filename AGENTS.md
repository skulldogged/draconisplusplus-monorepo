# AGENTS.md - Draconis++ Monorepo Coding Agent Guide

This file provides essential context for AI coding agents working in this repository.

## Project Overview

Draconis++ is a cross-platform system information tool written in **C++26**. It supports Windows, macOS, Linux, FreeBSD, OpenBSD, NetBSD, Haiku, and SerenityOS.

This monorepo contains:
- `core/` - The main C++ library and CLI
- `plugins/` - Plugin implementations (weather, now_playing, formats)
- `bindings/` - Language bindings (Rust, Python, Lua, C#, Kotlin, C3)
- `c-api/` - C API wrapper for all bindings

## Build System

**Primary**: Meson (1.1+) with Ninja
**Task Runner**: just (recommended)

### Essential Commands

```bash
just setup              # meson setup build
just build              # meson compile -C build
just test               # Run all tests
just test-one NAME      # Run single test (e.g., just test-one "Types")
just format             # clang-format on core/src core/include plugins
just lint               # clang-tidy -p build

# Direct meson for single tests:
meson test -C build "Error Handling"
```

## Code Style Guidelines

### Formatting
- **Style**: Chromium-based (`.clang-format`)
- **Indentation**: 2 spaces, no column limit
- **Braces**: Attach (same line)
- **Namespace indentation**: All content indented

### Naming Conventions

| Element             | Style        | Example                    |
|---------------------|--------------|----------------------------|
| Classes/Structs     | PascalCase   | `SystemInfo`, `DracError`  |
| Global Functions    | PascalCase   | `GetCpuInfo()`, `GetMemInfo()` |
| Member Methods      | camelBack    | `parseInto()`, `getValue()` |
| Local Variables     | camelBack    | `cpuCount`, `errorMsg`     |
| Private Members     | m_camelBack  | `m_cache`, `m_config`      |
| Static Constants    | UPPER_CASE   | `MAX_BUFFER_SIZE`          |
| Namespaces          | lowercase    | `draconis::core::system`   |
| Enum Values         | CamelCase    | `DracErrorCode::NotFound`  |

### Type System (`core/include/Drac++/Utils/Types.hpp`)

```cpp
// Primitives: u8-u64, i8-i64, f32, f64, usize, isize
// Strings: String, StringView, WString (Windows)
// Containers: Vec<T>, Array<T,N>, Span<T>, Map<K,V>, UnorderedMap<K,V>
// Pointers: UniquePointer<T>, SharedPointer<T>
// Results: Option<T>, Result<T,E>, Err<E>, None, Some(val)
```

### Import Patterns

**Header files** - fully qualified names:
```cpp
#pragma once
#include <Drac++/Utils/Types.hpp>
namespace draconis::core {
  auto GetData() -> utils::types::Result<utils::types::String>;
}
```

**Source files** - `using namespace`:
```cpp
using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;
```

**Include order** (clang-format auto-sorts):
1. System `<...>` 2. `<Drac++/...>` 3. `<Drac++/Utils/...>` 4. `"..."`

### Function Declarations

```cpp
auto GetSystemName() -> Result<String>;
[[nodiscard]] auto CalculateHash(StringView input) -> u64;
auto SafeOperation() noexcept -> bool;
```

### Error Handling (`core/include/Drac++/Utils/Error.hpp`)

**Prefer `Result<T>` over exceptions:**
```cpp
// Return errors
ERR(NotFound, "Resource not found");
ERR_FMT(IoError, "Failed: {}", path);

// Propagate errors (Rust-style)
String content = TRY(ReadFile("config.toml"));
TRY_VOID(ValidateConfig());  // For Result<void>

// Return success
return Result<String>{"success"};
return {};  // For Result<void>
```

### Logging (`core/include/Drac++/Utils/Logging.hpp`)

```cpp
debug_log("Debug: {}", value);
info_log("Info message");
warn_log("Warning: {}", issue);
error_log("Error occurred");
error_at(someError);  // Log DracError with source location
```

## Project Structure

```
core/
  include/Drac++/       # Public API headers
    Core/, Services/, Utils/
  src/
    Lib/                # Library implementation
      Core/             # Cross-platform core
      OS/               # Platform-specific (.cpp, macOS/*.mm)
    CLI/                # Command-line interface
      Config/, Core/, UI/
  tests/                # Unit tests (boost.ut)

plugins/                # Plugin implementations
  weather/
  now_playing/
  json_format/
  markdown_format/
  yaml_format/

bindings/               # Language bindings (all use C API)
  rust/
  python/
  lua/
  csharp/
  kotlin/
  c3/

c-api/                  # C API wrapper
  include/draconis_c.h
  src/draconis_c.cpp
```

## Static Plugin System

Plugins can be statically linked via `DRAC_STATIC_PLUGINS` environment variable:

```bash
DRAC_STATIC_PLUGINS=now_playing,weather
```

All bindings use the C API (`c-api/include/draconis_c.h`), not the C++ API directly.

## Testing (Boost.UT)

```cpp
#include <boost/ut.hpp>
using namespace boost::ut;
using namespace draconis::utils::types;

auto main() -> int {
  "test name"_test = [] -> void {
    expect(condition);
  };
  return 0;
}
```

## Platform-Specific Code

```cpp
#ifdef __linux__
// Linux
#elifdef _WIN32
// Windows
#elifdef __APPLE__
// macOS
#endif
```

**Build macros**: `DRAC_ARCH_X86_64`, `DRAC_ARCH_AARCH64`, `DRAC_ARCH_ARM`, `DRAC_ARCH_X86`
`DRAC_ARCH_64BIT`, `DRAC_DEBUG`, `DRAC_ENABLE_CACHING`, `DRAC_ENABLE_PLUGINS`

## Commit Messages

Use [Conventional Commits](https://www.conventionalcommits.org/):
```
feat: add battery level monitoring
fix: correct memory leak in cache manager
refactor: simplify OS detection logic
```

## Key Dependencies

- **mimalloc** - allocator
- **glaze** - JSON/BEVE
- **magic_enum** - enum reflection
- **boost.ut** - testing
- **ankerl::unordered_dense** - hash maps
