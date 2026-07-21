{self}: {
  config,
  lib,
  pkgs,
  ...
}:
with lib; let
  cfg = config.programs.draconisplusplus or {};

  tomlFormat = pkgs.formats.toml {};

  defaultPackage = self.packages.${pkgs.stdenv.hostPlatform.system}.default;

  stdenvHost = pkgs.stdenv;
  isLinux = stdenvHost.isLinux or false;
  isDarwin = stdenvHost.isDarwin or false;

  managerEnumMap = {
    cargo = "Cargo";
    nix = "Nix";
    apk = "Apk";
    dpkg = "Dpkg";
    moss = "Moss";
    pacman = "Pacman";
    rpm = "Rpm";
    xbps = "Xbps";
    homebrew = "Homebrew";
    macports = "Macports";
    winget = "Winget";
    chocolatey = "Chocolatey";
    scoop = "Scoop";
    pkgng = "PkgNg";
    pkgsrc = "PkgSrc";
    haikupkg = "HaikuPkg";
  };

  selectedManagers = map (pkg: "services::packages::Manager::${managerEnumMap.${pkg}}") cfg.packageManagers;

  packageManagerValue =
    if selectedManagers == []
    then "services::packages::Manager::None"
    else builtins.concatStringsSep " | " selectedManagers;

  logoAttrs = filterAttrs (_: v: v != null) {
    path = cfg.logo.path;
    protocol = cfg.logo.protocol;
    width = cfg.logo.width;
    height = cfg.logo.height;
  };

  # Generate C++ logo config for precompiled builds
  logoConfigCode = ''
    inline constexpr PrecompiledLogo DRAC_LOGO = {
      ${lib.optionalString (cfg.logo.path != null) ".path = \"${escapeCppString cfg.logo.path}\","}
      ${lib.optionalString (cfg.logo.protocol != null) ".protocol = \"${cfg.logo.protocol}\","}
      ${lib.optionalString (cfg.logo.width != null) ".width = ${toString cfg.logo.width},"}
      ${lib.optionalString (cfg.logo.height != null) ".height = ${toString cfg.logo.height},"}
    };
  '';

  defaultLayout = [
    {
      name = "intro";
      rows = [{key = "date";}];
    }
    {
      name = "system";
      rows = [
        {key = "host";}
        {key = "os";}
        {key = "kernel";}
      ];
    }
    {
      name = "hardware";
      rows = [
        {key = "cpu";}
        {key = "gpu";}
        {key = "ram";}
        {key = "disk";}
        {key = "uptime";}
      ];
    }
    {
      name = "software";
      rows = [
        {key = "shell";}
        {key = "packages";}
      ];
    }
    {
      name = "session";
      rows = [
        {key = "de";}
        {key = "wm";}
        {key = "playing";}
      ];
    }
  ];

  sanitizeLayoutName = name: lib.toUpper (lib.strings.replaceStrings [" " "-" "."] ["_" "_" "_"] name);

  escapeCppString = s:
    builtins.replaceStrings
    ["\\" "\""]
    [
      "\\\\"
      "\\\""
    ]
    (toString s);

  layoutRowToHpp = row: let
    autoWrapVal =
      if (row.autoWrap or false)
      then "true"
      else "false";
    labelVal = let
      label = row.label or null;
    in
      if label == null
      then "nullptr"
      else "\"${escapeCppString label}\"";
    iconVal = let
      icon = row.icon or null;
    in
      if icon == null
      then "nullptr"
      else "\"${escapeCppString icon}\"";
    colorVal = let
      color = row.color or null;
    in
      if color == null
      then "LogColor::White"
      else "LogColor::${color}";
    keyVal = "\"${escapeCppString row.key}\"";
  in "Row(${keyVal}, ${autoWrapVal}, ${colorVal}, ${labelVal}, ${iconVal})";

  layoutGroupHppEntries =
    map (
      group: let
        rows = group.rows or [];
        arrayName = "DRAC_UI_${sanitizeLayoutName group.name}_ROWS";
      in {
        inherit arrayName;
        name = group.name;
        rowsCode = ''
          inline constexpr std::array<PrecompiledLayoutRow, ${toString (builtins.length rows)}> ${arrayName} = {
            ${builtins.concatStringsSep ",\n              " (map (row: layoutRowToHpp row) rows)}
          };
        '';
      }
    )
    cfg.layout;

  layoutGroupsHppCode = builtins.concatStringsSep "\n\n        " (map (g: g.rowsCode) layoutGroupHppEntries);

  layoutArrayHppCode = ''
    inline constexpr std::array<PrecompiledLayoutGroup, ${toString (builtins.length layoutGroupHppEntries)}> DRAC_UI_LAYOUT = {
      ${builtins.concatStringsSep "\n          " (map (g: ''Group("${g.name}", ${g.arrayName}),'') layoutGroupHppEntries)}
    };
  '';

  layoutToml =
    map (
      group: {
        name = group.name;
        rows =
          map (
            row:
              filterAttrs (_: v: v != null) {
                key = row.key;
                label = row.label or null;
                icon = row.icon or null;
                color = row.color or null;
                auto_wrap = row.autoWrap or false;
              }
          )
          (group.rows or []);
      }
    )
    cfg.layout;

  effectiveStaticPlugins =
    if cfg.staticPlugins != []
    then cfg.staticPlugins
    else lib.optional (cfg.pluginMode == "static") "all";

  configHpp =
    pkgs.writeText "config.hpp"
    # cpp
    ''
      #pragma once

      #if DRAC_PRECOMPILED_CONFIG

        #include <array>
        #include <Drac++/Config/PrecompiledLayout.hpp>

        #if DRAC_ENABLE_PACKAGECOUNT
          #include <Drac++/Services/Packages.hpp>
        #endif

      namespace draconis::config {
        constexpr const char* DRAC_USERNAME = "${cfg.username}";

        ${logoConfigCode}

        #if DRAC_ENABLE_PACKAGECOUNT
        constexpr services::packages::Manager DRAC_ENABLED_PACKAGE_MANAGERS = ${packageManagerValue};
        #endif

        ${layoutGroupsHppCode}

        ${layoutArrayHppCode}
      }

      #endif
    '';

  withPlugins = import ./with-plugins.nix {inherit lib;};

  draconisPkg = withPlugins {
    package = cfg.package;
    inherit (cfg) pluginDirs pluginPackages;
    staticPlugins = effectiveStaticPlugins;
    postPatch = lib.optionalString (cfg.configFormat == "hpp") ''
      cp ${configHpp} ./config.hpp
    '';
    mesonFlags = [
      "-Dprecompiled_config=${
        if cfg.configFormat == "hpp"
        then "true"
        else "false"
      }"
      "-Dcaching=${
        if cfg.enableCaching
        then "enabled"
        else "disabled"
      }"
      "-Dpackagecount=${
        if cfg.enablePackageCount
        then "enabled"
        else "disabled"
      }"
      "-Dplugins=${
        if cfg.enablePlugins
        then "enabled"
        else "disabled"
      }"
      "-Dpugixml=${
        if cfg.usePugixml
        then "enabled"
        else "disabled"
      }"
    ];
  };
in {
  options.programs.draconisplusplus = {
    enable = mkEnableOption "draconis++";

    package = mkOption {
      type = types.package;
      default = defaultPackage;
      description = "The base draconis++ package.";
    };

    configFormat = mkOption {
      type = types.enum ["toml" "hpp"];
      default = "toml";
      description = "The configuration format to use.";
    };

    username = mkOption {
      type = types.str;
      default = config.home.username // "User";
      description = "Username to display in the application.";
    };

    language = mkOption {
      type = types.str;
      default = "en";
      description = "Language code for localization (e.g., \"en\", \"es\").";
    };

    logo = mkOption {
      description = "Logo configuration (used for kitty/kitty-direct/iTerm2 inline images).";
      default = {
        path = null;
        protocol = "kitty";
        width = null;
        height = null;
      };
      type = types.submodule {
        options = {
          path = mkOption {
            type = types.nullOr types.path;
            default = null;
            description = "Path to the logo image.";
          };

          protocol = mkOption {
            type = types.enum ["kitty" "kitty-direct" "iterm2"];
            default = "kitty";
            description = "Logo protocol to use.";
          };

          width = mkOption {
            type = types.nullOr types.int;
            default = null;
            description = "Optional logo width.";
          };

          height = mkOption {
            type = types.nullOr types.int;
            default = null;
            description = "Optional logo height.";
          };
        };
      };
    };

    enablePlugins = mkOption {
      type = types.bool;
      default = true;
      description = "Enable plugin system.";
    };

    pluginConfigs = mkOption {
      type = types.attrsOf types.attrs;
      default = {};
      description = ''
        Per-plugin configuration written under [plugins.<name>] in config.toml.
        Keys and values are passed through directly to the TOML generator.
      '';
      example = literalExpression ''
        {
          weather = {
            enabled = true;
            provider = "openmeteo";
            coords = {
              lat = 40.7128;
              lon = -74.0060;
            };
          };
        }
      '';
    };

    packageManagers = mkOption {
      type = types.listOf (types.enum (
        ["cargo"]
        ++ lib.optionals isLinux ["apk" "dpkg" "moss" "pacman" "rpm" "xbps" "nix"]
        ++ lib.optionals isDarwin ["homebrew" "macports" "nix"]
      ));
      default = [];
      description = "List of package managers to check for package counts.";
    };

    staticPlugins = mkOption {
      type = types.listOf types.str;
      default = [];
      description = ''
        Plugins to compile statically into the binary. Accepts any discovered
        plugin name from pluginDirs or pluginPackages, or "all" for every
        discovered plugin. Names are validated by the build.
      '';
      example = literalExpression ''["weather" "now_playing"]'';
    };

    pluginMode = mkOption {
      type = types.enum ["dynamic" "static"];
      default = "dynamic";
      description = ''
        How packaged plugins are built when staticPlugins is empty. In static
        mode, every plugin contained in pluginDirs and pluginPackages is
        compiled into Draconis++. An explicit staticPlugins list takes
        precedence for backwards compatibility.
      '';
    };

    pluginDirs = mkOption {
      type = types.listOf types.path;
      default = [];
      description = ''
        External plugin roots passed to the build via -Dplugin_dirs=.
        Each subdirectory containing a plugin.json is discovered as a plugin,
        so third-party plugins can be consumed without their own Nix flake.
      '';
      example = literalExpression ''[./my-draconis-plugins]'';
    };

    pluginPackages = mkOption {
      type = types.listOf types.package;
      default = [];
      description = ''
        Nix packages that expose a plugin root. Each package path is passed to
        the build via -Dplugin_dirs=, so it should contain plugin directories as
        direct children. This is intended for flake-packaged plugin collections
        such as the official Draconis++ plugins. Packages may expose
        passthru.pluginBuildInputs; those inputs are added to the Draconis++
        build environment so plugin manifest dependencies can resolve.
      '';
      example = literalExpression ''[inputs.draconisplusplus-plugins.packages.''${pkgs.stdenv.hostPlatform.system}.all]'';
    };

    pluginAutoLoad = mkOption {
      type = types.listOf types.str;
      default = [];
      description = "Plugin names to auto-load at runtime.";
    };

    layout = mkOption {
      type = types.listOf (types.submodule {
        options = {
          name = mkOption {
            type = types.str;
            description = "Display name for the layout group (not rendered, for readability).";
          };

          rows = mkOption {
            type = types.listOf (types.submodule {
              options = {
                key = mkOption {
                  type = types.str;
                  description = "Data key to render (e.g., \"cpu\", \"plugin.weather\").";
                };

                label = mkOption {
                  type = types.nullOr types.str;
                  default = null;
                  description = "Optional label override.";
                };

                icon = mkOption {
                  type = types.nullOr types.str;
                  default = null;
                  description = "Optional icon override.";
                };

                color = mkOption {
                  type = types.nullOr types.str;
                  default = null;
                  description = "Optional value color (matches LogColor enum, e.g., \"Magenta\").";
                };

                autoWrap = mkOption {
                  type = types.bool;
                  default = false;
                  description = "Enable automatic word wrapping for this row.";
                };
              };
            });
            default = [];
            description = "Rows to render within this group.";
          };
        };
      });
      default = defaultLayout;
      description = "UI layout groups and rows for the draconis++ dashboard.";
      example = literalExpression ''
        [
          {
            name = "intro";
            rows = [
              { key = "date"; }
              { key = "plugin.weather"; color = "Magenta"; autoWrap = true; }
            ];
          }
          {
            name = "nowplaying";
            rows = [
              { key = "plugin.now_playing"; color = "Magenta"; autoWrap = true; }
            ];
          }
        ]
      '';
    };

    enablePackageCount = mkOption {
      type = types.bool;
      default = true;
      description = "Enable getting package count.";
    };

    enableCaching = mkOption {
      type = types.bool;
      default = true;
      description = "Enable caching functionality.";
    };

    usePugixml = mkOption {
      type = types.bool;
      default = false;
      description = "Use pugixml to parse XBPS package metadata. Required for package count functionality on Void Linux.";
    };
  };

  config = mkIf cfg.enable {
    home.packages = [draconisPkg];

    # Dynamic plugins are installed under the package's lib dir, which the
    # FHS search paths never cover on Nix; DRAC_PLUGIN_PATH points the
    # plugin loader at them.
    home.sessionVariables = lib.optionalAttrs (cfg.enablePlugins && cfg.staticPlugins == []) {
      DRAC_PLUGIN_PATH = "${draconisPkg}/lib/draconis++/plugins";
    };

    xdg.configFile = lib.optionalAttrs (cfg.configFormat == "toml") {
      "draconis++/config.toml" = {
        source = tomlFormat.generate "config.toml" (
          {
            general = {
              name = cfg.username;
              language = cfg.language;
            };
            packages.enabled = cfg.packageManagers;
            plugins =
              {
                enabled = cfg.enablePlugins;
                auto_load = cfg.pluginAutoLoad;
              }
              // cfg.pluginConfigs;
            ui = {layout = layoutToml;};
          }
          // lib.optionalAttrs (logoAttrs != {}) {logo = logoAttrs;}
        );
      };
    };

    assertions = [
      {
        assertion = !(cfg.usePugixml && !cfg.enablePackageCount);
        message = "usePugixml should only be enabled when enablePackageCount is also enabled.";
      }
      {
        assertion = !(cfg.pluginAutoLoad != [] && !cfg.enablePlugins);
        message = "Plugins must be enabled to auto-load plugins.";
      }
      {
        assertion = !(cfg.staticPlugins != [] && !cfg.enablePlugins);
        message = "Plugins must be enabled to compile static plugins.";
      }
      {
        assertion = cfg.pluginConfigs == {} || cfg.configFormat == "toml";
        message = "pluginConfigs are written as TOML runtime config. For configFormat = \"hpp\", use a configured plugin package that generates its own per-plugin config.hpp.";
      }
    ];
  };
}
