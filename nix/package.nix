{
  pkgs,
  lib,
  self,
  ...
}: let

  llvmPackages = pkgs.llvmPackages_20;

  stdenv = with pkgs;
    (
      if hostPlatform.isLinux
      then stdenvAdapters.useMoldLinker
      else lib.id
    )
    llvmPackages.stdenv;

  boostUt = pkgs.callPackage ./boost-ut.nix {};

  deps = with pkgs;
    [
      (glaze.overrideAttrs rec {
        version = "6.1.0";

        src = pkgs.fetchFromGitHub {
          owner = "stephenberry";
          repo = "glaze";
          tag = "v${version}";
          hash = "sha256-H1paMc0LH743aMHCO/Ocp96SaaoXLcl/MDmmbtSJG+Q=";
        };
      })
      boostUt
    ]
    ++ (with pkgs.pkgsStatic; [
      curl
      mimalloc
      (magic-enum.overrideAttrs (old: {
        doCheck = false;
        cmakeFlags = (old.cmakeFlags or []) ++ ["-DMAGIC_ENUM_OPT_BUILD_TESTS=OFF"];
      }))
      sqlitecpp
      boostUt
    ])
    ++ darwinPkgs
    ++ linuxPkgs;

  darwinPkgs = lib.optionals stdenv.isDarwin (with pkgs.pkgsStatic; [
    libiconv
    apple-sdk_15
  ] ++ [
    pkgs.darwin.sigtool
  ]);

  linuxPkgs = lib.optionals stdenv.isLinux (with pkgs;
    [
      valgrind
    ]
    ++ (with pkgsStatic; [
      (dbus.overrideAttrs (old: let
        filterAuditApparmor = builtins.filter (p:
          let name = p.pname or ""; in
          name != "audit" && name != "libapparmor"
        );
      in {
        buildInputs = filterAuditApparmor (old.buildInputs or []);
        propagatedBuildInputs = filterAuditApparmor (old.propagatedBuildInputs or []);
        mesonFlags = builtins.filter (f:
          !(lib.hasPrefix "-Dlibaudit" f) && !(lib.hasPrefix "-Dapparmor" f)
        ) (old.mesonFlags or []) ++ [
          "-Dlibaudit=disabled"
          "-Dapparmor=disabled"
        ];
        configureFlags = builtins.filter (f:
          !(lib.hasPrefix "--enable-apparmor" f) && !(lib.hasPrefix "--enable-libaudit" f)
        ) (old.configureFlags or []) ++ [
          "--disable-apparmor"
          "--disable-libaudit"
        ];
      }))
      pugixml
      xorg.libxcb
      wayland
    ]));

  mkDraconisPackage = lib.makeOverridable ({
    native,
  }:
    stdenv.mkDerivation {
      name =
        "draconis++"
        + (
          if native
          then "-native"
          else "-generic"
        );
      version = "0.1.0";
      src = self;

      nativeBuildInputs = with pkgs;
        [
          cmake
          gitMinimal
          meson
          ninja
          pkg-config
          python3
        ]
        ++ lib.optional stdenv.isLinux xxd;

      buildInputs = deps;

      mesonFlags = [
        "-Dbuild_examples=false"
        (lib.optionalString stdenv.isLinux "-Duse_linked_pci_ids=true")
      ];

      configurePhase = ''
        meson setup build --buildtype=release $mesonFlags
      '';

      buildPhase =
        lib.optionalString stdenv.isLinux ''
          cp ${pkgs.pciutils}/share/pci.ids pci.ids
          chmod +w pci.ids
          ld -r -b binary -o pci_ids.o pci.ids
          rm pci.ids
        ''
        + ''
          meson compile -C build
        '';

      checkPhase = ''
        meson test -C build --print-errorlogs
      '';

      installPhase = ''
        mkdir -p $out/bin $out/lib
        mv build/core/src/CLI/draconis++ $out/bin/draconis++
        mv build/core/src/Lib/libdrac++.a $out/lib/
        mkdir -p $out/include
        cp -r core/include/Drac++ $out/include/
      '';

      postFixup = lib.optionalString stdenv.isDarwin ''
        echo "Signing binary..."
        codesign --force -s - --identifier com.apple.draconisplusplus $out/bin/draconis++
      '';

      NIX_ENFORCE_NO_NATIVE =
        if native
        then 0
        else 1;
    });
in {
  "generic" = mkDraconisPackage {native = false;};
  "native" = mkDraconisPackage {native = true;};
}
