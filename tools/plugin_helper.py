#!/usr/bin/env python3
"""Plugin build helper for Draconis++.

This script is the single source of truth for plugin manifest (plugin.json)
handling. It is invoked by Meson at setup time and can also be used directly
to scaffold new plugins.

Subcommands:
  discover <dir>...                List discovered plugins as "name|path" lines.
                                   A plugin is any direct subdirectory of <dir>
                                   that contains a plugin.json manifest.
  info <plugin_dir> --platform X   Print machine-readable build info for one
                                   plugin, filtered for the given host platform.
  registry [<class>...]            Generate the StaticPluginInit.cpp translation
                                   unit that registers the given plugin classes.
  new <name> [--dir DIR]           Scaffold a new plugin directory.

plugin.json schema:
  {
    "name": "my_plugin",              // defaults to the directory name
    "class": "MyPlugin",              // C++ class passed to DRAC_PLUGIN(); defaults to name
    "description": "...",
    "platforms": ["linux", "darwin"], // or "all" (default); "platform" (string) also accepted
    "codesign": false,                // macOS: binary needs ad-hoc codesigning
    "sources": [                      // extra sources; <name>.cpp is implicit if present
      "extra.cpp",
      { "file": "macos_impl.mm", "platforms": ["darwin"] }
    ],
    "deps": [                         // pkg-config / system dependencies
      { "name": "libcurl", "include_type": "system", "static": true, "platforms": ["linux"] }
    ]
  }

Platform names match Meson's host_machine.system(): linux, darwin, windows,
freebsd, netbsd, openbsd, dragonfly, haiku, serenity.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def fail(message: str) -> None:
    print(f"plugin_helper: error: {message}", file=sys.stderr)
    sys.exit(1)


def load_manifest(plugin_dir: Path) -> dict:
    manifest_path = plugin_dir / "plugin.json"
    if not manifest_path.is_file():
        fail(f"no plugin.json found in {plugin_dir}")
    try:
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail(f"{manifest_path}: invalid JSON: {exc}")
    if not isinstance(data, dict):
        fail(f"{manifest_path}: manifest must be a JSON object")
    return data


def manifest_name(plugin_dir: Path, data: dict) -> str:
    return data.get("name", plugin_dir.name)


def platform_matches(platforms, host: str) -> bool:
    """platforms may be missing/None, "all", a platform string, or a list."""
    if not platforms:
        return True
    if isinstance(platforms, str):
        platforms = [platforms]
    return "all" in platforms or host in platforms


def cmd_discover(args: argparse.Namespace) -> None:
    seen: dict[str, Path] = {}
    for search_dir in args.dirs:
        base = Path(search_dir)
        if not base.is_dir():
            continue
        for sub in sorted(base.iterdir()):
            if not (sub / "plugin.json").is_file():
                continue
            data = load_manifest(sub)
            name = manifest_name(sub, data)
            if name in seen:
                fail(
                    f'duplicate plugin name "{name}": found in both '
                    f"{seen[name]} and {sub}"
                )
            seen[name] = sub
            print(f"{name}|{sub.resolve()}")


def cmd_info(args: argparse.Namespace) -> None:
    plugin_dir = Path(args.plugin_dir)
    data = load_manifest(plugin_dir)
    name = manifest_name(plugin_dir, data)
    host = args.platform

    print(f"NAME|{name}")
    print(f"CLASS|{data.get('class', name)}")

    supported = platform_matches(data.get("platforms", data.get("platform")), host)
    print(f"SUPPORTED|{'yes' if supported else 'no'}")
    print(f"CODESIGN|{'yes' if data.get('codesign', False) else 'no'}")

    if not supported:
        return

    sources: list[Path] = []
    main_source = plugin_dir / f"{name}.cpp"
    if main_source.is_file():
        sources.append(main_source)

    for entry in data.get("sources", []):
        if isinstance(entry, str):
            entry = {"file": entry}
        if not platform_matches(entry.get("platforms"), host):
            continue
        source = plugin_dir / entry["file"]
        if not source.is_file():
            fail(f'plugin "{name}": source file not found: {source}')
        if source not in sources:
            sources.append(source)

    if not sources:
        fail(
            f'plugin "{name}": no sources for platform "{host}" '
            f'(expected {main_source} or entries in the "sources" list)'
        )

    for source in sources:
        print(f"SRC|{source.resolve()}")

    for dep in data.get("deps", []):
        if not platform_matches(dep.get("platforms"), host):
            continue
        dep_name = dep.get("name")
        if not dep_name:
            fail(f'plugin "{name}": dependency entry is missing "name"')
        include_type = dep.get("include_type", "preserve")
        static = "true" if dep.get("static", False) else "false"
        print(f"DEP|{dep_name}|{include_type}|{static}")


def cmd_registry(args: argparse.Namespace) -> None:
    classes = args.classes
    lines = [
        "// Auto-generated by tools/plugin_helper.py - do not edit",
        "#include <Drac++/Core/StaticPlugins.hpp>",
        "",
        "#include <cstddef>",
        "",
        "#if DRAC_ENABLE_PLUGINS",
        "",
    ]

    if classes:
        lines.append('extern "C" {')
        for cls in classes:
            lines.append(f"  void DracRegisterPlugin_{cls}();")
        lines.append("}")
        lines.append("")

    lines += [
        "namespace draconis::core::plugin {",
        "  auto DracInitStaticPlugins() -> std::size_t {",
    ]
    for cls in classes:
        lines.append(f"    DracRegisterPlugin_{cls}();")
    lines += [
        f"    return {len(classes)};",
        "  }",
        "} // namespace draconis::core::plugin",
        "",
        "#endif // DRAC_ENABLE_PLUGINS",
    ]

    print("\n".join(lines))


PLUGIN_TEMPLATE = """/**
 * @file @NAME@.cpp
 * @brief @LABEL@ plugin for Draconis++
 *
 * @details Generated by tools/plugin_helper.py. This plugin builds both as a
 * dynamic plugin (shared library loaded at runtime) and as a static plugin
 * (compiled into the binary via -Dstatic_plugins=@NAME@) without changes.
 */

#include <Drac++/Core/Plugin.hpp>

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

namespace {
  using namespace draconis::utils::types;
  using draconis::core::plugin::PluginContext;
  using draconis::core::plugin::PluginMetadata;
  using draconis::core::plugin::PluginType;

  class @CLASS@ final : public draconis::core::plugin::IInfoProviderPlugin {
   public:
    @CLASS@() {
      m_metadata = {
        .name         = "@NAME@",
        .version      = "0.1.0",
        .author       = "Your Name",
        .description  = "Describe what this plugin provides",
        .type         = PluginType::InfoProvider,
        .dependencies = {},
      };
    }

    [[nodiscard]] auto getMetadata() const -> const PluginMetadata& override {
      return m_metadata;
    }

    auto initialize(const PluginContext& ctx, ::PluginCache& cache) -> Result<Unit> override {
      (void)ctx;
      (void)cache;
      // Read configuration from ctx.configDir / "@NAME@.toml" here if needed.
      m_ready = true;
      return {};
    }

    auto shutdown() -> Unit override {
      m_ready = false;
    }

    [[nodiscard]] auto isReady() const -> bool override {
      return m_ready;
    }

    [[nodiscard]] auto isEnabled() const -> bool override {
      return true;
    }

    [[nodiscard]] auto getProviderId() const -> String override {
      return "@NAME@";
    }

    auto collectData(::PluginCache& cache) -> Result<Unit> override {
      (void)cache;
      // Gather your data here. Use cache.get<T>() / cache.set<T>() to avoid
      // repeating expensive lookups between runs.
      m_value = "Hello from @NAME@";
      return {};
    }

    [[nodiscard]] auto toJson() const -> Result<String> override {
      return std::format(R"({{"value":"{}"}})", m_value);
    }

    [[nodiscard]] auto getFields() const -> Map<String, String> override {
      return { { "@NAME@_value", m_value } };
    }

    [[nodiscard]] auto getDisplayValue() const -> Result<String> override {
      return m_value;
    }

    [[nodiscard]] auto getDisplayIcon() const -> String override {
      return "  ";
    }

    [[nodiscard]] auto getDisplayLabel() const -> String override {
      return "@LABEL@";
    }

    [[nodiscard]] auto getLastError() const -> Option<String> override {
      return None;
    }

   private:
    PluginMetadata m_metadata;
    String         m_value;
    bool           m_ready = false;
  };
} // namespace

DRAC_PLUGIN(@CLASS@)
"""

MANIFEST_TEMPLATE = """{
  "name": "@NAME@",
  "class": "@CLASS@",
  "description": "Describe what this plugin provides",
  "platforms": ["all"],
  "deps": []
}
"""


def cmd_new(args: argparse.Namespace) -> None:
    name = args.name
    if not name.replace("_", "").isalnum() or name[0].isdigit():
        fail(f'invalid plugin name "{name}" (use lowercase letters, digits, underscores)')

    plugin_dir = Path(args.dir) / name
    if plugin_dir.exists():
        fail(f"{plugin_dir} already exists")

    class_name = "".join(part.capitalize() for part in name.split("_"))
    if not class_name.endswith("Plugin"):
        class_name += "Plugin"
    label = name.replace("_", " ").title()

    def render(template: str) -> str:
        return (
            template.replace("@NAME@", name)
            .replace("@CLASS@", class_name)
            .replace("@LABEL@", label)
        )

    plugin_dir.mkdir(parents=True)
    (plugin_dir / "plugin.json").write_text(render(MANIFEST_TEMPLATE), encoding="utf-8")
    (plugin_dir / f"{name}.cpp").write_text(render(PLUGIN_TEMPLATE), encoding="utf-8")

    plugin_root = plugin_dir.parent
    plugin_dirs_arg = f" -Dplugin_dirs={plugin_root}" if plugin_root != Path("plugins") else ""

    print(f"Created plugin '{name}' in {plugin_dir}")
    print("Build it:")
    print(f"  dynamic: meson setup build{plugin_dirs_arg} && meson compile -C build {name}")
    print(f"  static:  meson setup build{plugin_dirs_arg} -Dstatic_plugins={name} && meson compile -C build")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    p_discover = subparsers.add_parser("discover", help="list plugins found in the given directories")
    p_discover.add_argument("dirs", nargs="+")
    p_discover.set_defaults(func=cmd_discover)

    p_info = subparsers.add_parser("info", help="print build info for a plugin directory")
    p_info.add_argument("plugin_dir")
    p_info.add_argument("--platform", required=True)
    p_info.set_defaults(func=cmd_info)

    p_registry = subparsers.add_parser("registry", help="generate the static plugin registration source")
    p_registry.add_argument("classes", nargs="*")
    p_registry.set_defaults(func=cmd_registry)

    p_new = subparsers.add_parser("new", help="scaffold a new plugin")
    p_new.add_argument("name")
    p_new.add_argument("--dir", default="plugins")
    p_new.set_defaults(func=cmd_new)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
