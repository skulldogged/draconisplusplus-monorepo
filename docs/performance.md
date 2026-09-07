# Collection and reuse

The CLI selects core fields from compact placeholders or a configured UI layout.
Normal layout keys are case-insensitive; `package` aliases `packages`. A desktop
environment row also requests the window manager so the existing duplicate-row
suppression still works. Linux retains OS identification for distro icons and
logos. Windows and macOS logos do not require an otherwise unused OS query.

The automatic layout still includes all enabled providers. Doctor and formatter
output explicitly request full collection, independently of a compact template
or custom layout.

Plugin placeholders and `plugin.<provider>[.<field>]` layout rows select matching
provider IDs. `weather` and `playing` compact aliases select `weather` and
`now_playing`, respectively. Underscores in provider and field IDs are preserved.
IDs are obtained from provider metadata access before initialization, so plugin
constructors and `getProviderId()` should be cheap and IDs should remain stable.
Discovery may construct candidates, including temporarily loading dynamic
modules, to identify them. Only matching providers are initialized and collected;
metadata is reused until discovery is refreshed, a plugin is unloaded, or the
manager shuts down. Explicit `auto_load` entries retain eager initialization.
Core-only CLI requests skip plugin discovery when no plugin command or auto-load
entry requires it.

On Windows, host and CPU-model values use the existing memory-only cache policy:
their direct queries are cheaper than a disk-cache read. Expensive stable values
such as GPU discovery retain persistence. Cache bypass and invalidation semantics
are unchanged.

Keep a `CacheManager` alive across repeated C++ requests, or retain its equivalent
C/binding handle. The HTTP example now does this. Its HTML and CSS are loaded at
startup; restart the example to reload those files. Live memory/disk reads and
the existing source-specific expiration policies still apply.

The MCP example derives the Windows primary display from its returned display
list and uses `GetNetworkSnapshot()` for all interfaces plus the route-selected
primary interface. Windows obtains that pair from one fresh adapter enumeration.
Other platforms retain their existing collector behavior. Snapshots are scoped
to one response, not retained as permanent topology caches.

The container plugin owns bounded CURL sessions keyed by URL base and Unix socket
path. It serializes access, resets request options between calls, reconnects when
needed, and closes sessions at shutdown. This retains transport state, not
container-count results. Windows named-pipe transport is unchanged. See libcurl's
[reset semantics](https://curl.se/libcurl/c/curl_easy_reset.html) and
[handle threading rules](https://curl.se/libcurl/c/threadsafe.html).
