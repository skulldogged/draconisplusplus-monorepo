{lib}: {
  package,
  pluginDirs ? [],
  pluginPackages ? [],
  staticPlugins ? [],
  mesonFlags ? [],
  postPatch ? "",
}: let
  pluginRoots = pluginDirs ++ pluginPackages;

  metadataFor = pluginPackage: pluginPackage.passthru or {};

  namesFor = pluginPackage: let
    metadata = metadataFor pluginPackage;
  in
    if metadata ? pluginNames
    then metadata.pluginNames
    else builtins.attrNames (metadata.pluginBuildInputsByName or {});

  selectedNamesFor = pluginPackage: let
    names = namesFor pluginPackage;
  in
    if staticPlugins == [] || builtins.elem "all" staticPlugins
    then names
    else builtins.filter (name: builtins.elem name staticPlugins) names;

  buildInputsFor = pluginPackage: let
    metadata = metadataFor pluginPackage;
  in
    if metadata ? pluginBuildInputsByName
    then
      lib.concatMap
      (name: metadata.pluginBuildInputsByName.${name} or [])
      (selectedNamesFor pluginPackage)
    else metadata.pluginBuildInputs or [];

  pluginBuildInputs = lib.unique (lib.concatMap buildInputsFor pluginPackages);
  knownPluginNames = lib.unique (lib.concatMap namesFor pluginPackages);
  requestedPluginNames = builtins.filter (name: name != "all") staticPlugins;
  unknownPluginNames = builtins.filter (name: !(builtins.elem name knownPluginNames)) requestedPluginNames;
  canValidatePluginNames = pluginDirs == [] && builtins.all (package: (metadataFor package) ? pluginNames) pluginPackages;
in
  assert lib.assertMsg (!canValidatePluginNames || unknownPluginNames == [])
  "Unknown Draconis++ static plugins: ${lib.concatStringsSep ", " unknownPluginNames}";
    package.overrideAttrs (oldAttrs: {
      postPatch = (oldAttrs.postPatch or "") + postPatch;

      buildInputs = lib.unique ((oldAttrs.buildInputs or []) ++ pluginBuildInputs);

      mesonFlags =
        (oldAttrs.mesonFlags or [])
        ++ mesonFlags
        ++ lib.optional (staticPlugins != []) "-Dstatic_plugins=${lib.concatStringsSep "," staticPlugins}"
        ++ lib.optional (pluginRoots != []) "-Dplugin_dirs=${lib.concatStringsSep "," (map toString pluginRoots)}";
    })
