{
  nixpkgs,
  self,
  system,
  lib,
  pluginsSrc ? null,
  ...
}: let
  pkgs = import nixpkgs {
    inherit system;
  };

  dracPackages = import ./package.nix {inherit pkgs self lib pluginsSrc;};

  muslPackages =
    if pkgs.stdenv.isLinux
    then import ./musl.nix {inherit pkgs nixpkgs self lib pluginsSrc;}
    else {};
in
  dracPackages
  // muslPackages
  // {default = dracPackages."generic";}
