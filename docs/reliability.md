# Reliability and compatibility contracts

The reliability changes require rebuilding the core and all C++ plugins together.
Dynamic plugins export `DracPluginAbiVersion()` returning 2; missing or mismatched
versions are rejected before factory invocation. This is a layout-version check,
not a portable C++ ABI: compiler, standard library, architecture, runtime linkage,
and SDK configuration must still match. Use a clean build directory when upgrading
an older SDK build, because old archive members can survive an incremental archive
update after the target composition changes.

## Ownership and concurrency

`PluginManager` returns owning `PluginHandle<T>` values and vector snapshots.
Hold `handle.lock()` across instance calls and copy borrowed values before releasing
the lock. Unload and manager shutdown remove manager ownership; retained handles
remain usable. Shutdown and the factory destructor run when the final instance
owner goes away, followed by release of its library. Manager callbacks run outside
the global manager lock. Failed initialization is retryable.

Manager initialization publishes readiness only after the directory scan succeeds.
Plugin context directories are created before invoking a plugin's initialization
callback; filesystem errors are returned and initialization can be retried. C
discovery copies metadata from temporary instances without initializing providers.
An incompatible or unreadable module can still appear by name with unavailable
metadata.

C handles own independent instances in both static and dynamic mode. Configure the
instance before initialization; initialization is idempotent. Loading from a path
uses that exact path. C calls translate exceptions to documented error results,
null, or an empty list, and clean partial output allocations. C callers must still
serialize destruction against concurrent operations on the same raw C handle.
Kotlin synchronizes close and every native getter; C# uses SafeHandle lifetime
protection. Dispose/close is idempotent in both wrappers.

Each plugin instance keeps a cache view alive. Binding that view to a C cache
manager retains the shared service even if the C cache wrapper is destroyed.
Services retain module leases until cached type-erased values have been destroyed.
On macOS, loaded plugin code stays mapped until process exit because frameworks can
retain asynchronous callback blocks after a provider times out. Plugin instances
still receive shutdown and destruction normally.

## Cache behavior

Core and plugin caches use one typed cache implementation with atomic disk writes,
same-key request coalescing, bounded disk reads, and confined keys. Plugin views
observe the creating host's bypass flag, including dynamic-library calls. Setting
`ignoreCache` bypasses reads and writes. Invalidating a key or all entries detaches
older in-flight work: existing waiters may receive their old result, while new
callers start fresh work and old work cannot republish.

The default and temporary policies expire after 60 seconds. An explicit stricter
TTL also constrains existing memory hits. Schema 2 stores creation time; older disk
entries are treated as misses. Weather uses a 600-second policy and keys by schema,
provider, location/coordinates, and units. Credentials are excluded from disk keys.

## Platform status

| Platform | Current contract | Validation for this change |
|---|---|---|
| Windows x64 | Native core, CLI, C runtime and bindings | Built and tested locally |
| Linux x86 / ARM | Architecture-gated CPUID; online sysfs CPU topology; corrected battery time path/units | Source changes; hosted CI added, not run locally |
| macOS | Correct power-source ownership; bounded MediaRemote wait; delayed-callback module retention | Source changes; hosted CI added, no macOS runtime test here |
| FreeBSD / NetBSD / DragonFly | Partial backend; missing getters return `NotSupported` | Experimental; no runtime validation here |
| OpenBSD | Explicit unsupported-getter baseline | Experimental; no runtime validation here |
| Haiku / SerenityOS | Partial backend; missing getters return `NotSupported` | Experimental; no runtime validation here |

Unsupported getters are deliberate errors, not zero-valued success. The Windows
WinGet counter now returns `NotSupported`; AppModel registration counts are not a
WinGet package inventory. Cargo counts installed package records from `.crates2.json`.
Linux core counts describe online system topology rather than process affinity.

## Build and verification

Meson installs the C++ SDK, generated configuration header, required headers and
pkg-config metadata, plus the standalone `draconis_c` runtime. Use `dependency('draconis')`
for C++ and `dependency('draconis-c')` for C. Windows archive resolution should go
through Meson/pkg-config dependency handling rather than assuming `.lib` filenames
for LLVM-created `.a` archives. Nix uses Meson's install plan. `native_tuning` is
opt-in; optional platform dependencies are detected in auto mode.

The installed `draconis.pc` records `sdk_buildtype` and, on Windows, `sdk_vscrt`.
The installed-consumer verifier uses those settings so its STL debug ABI and CRT
match the producer. Other C++ consumers must also use compatible build settings.

Run `meson test -C build --print-errorlogs`. The suite includes isolated cache
fixtures, malformed and large Windows topology records, static/dynamic plugin
ownership, failed-init retry, reentrancy, ABI rejection, and concurrent collection.
When configured sibling plugin roots contain `tests/test_weather_cache.cpp` or
`tests/test_pipe.cpp`, Meson also runs those offline fixtures. The Windows pipe test
covers valid response framing, truncated chunks, and a silent peer's three-second
deadline. Kotlin's Gradle build runs close/concurrency tests; the C# SmokeTest
example exercises native ownership. Python supports both integrated and standalone
wheel builds; Rust documents shared-runtime deployment in its binding README.

Hosted CI covers native Linux, Windows and macOS builds, core/dynamic-fixture tests,
Python and C# compilation, and installation. It must run after publication before
non-Windows validation can be claimed. Sibling plugin changes must be published
together with the ABI change. Full container-engine, desktop media, battery-hardware,
out-of-memory and race-sanitizer testing remain separate integration work.

## Performance measurements

The CLI benchmark reports diagnostic getter times separately from collection and
rendering; it includes lazy provider initialization and uses a steady clock.
It no longer sums duplicate collection phases into a misleading overall total.
The second collection pass is warm when caching is enabled.

Measure process startup separately from repeated requests. Custom layouts select
their required fields, provider requests initialize matching providers, and the
container plugin reuses endpoint-specific CURL sessions. See
[collection and reuse](performance.md) for selection, lifetime, and reload behavior.
These changes reduce application work; they do not establish an overall Windows
process-startup improvement.
