# Build sil-q with Nix.
#
# This flake provides reproducible builds of sil-q using cmake + ninja.
# The default build includes game data and path wrapping so the binary
# runs immediately.
#
# The mkSilQ function parameterizes the X11 frontend. Additional cmake flags
# can be threaded in as needed.
#
# Usage:
#
#   nix build                        # X11 + GCU (cmake Linux default)
#   nix build .#sil-q                # same
#   nix build .#sil-q-nox            # GCU only, no X11
#
# Enter a development shell (cmake, ninja, gcc, ncurses, libX11):
#
#   nix develop
#
{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};

      mkSilQ = { x11 ? true }:
        pkgs.stdenv.mkDerivation {
          pname = "sil-q";
          version = "1.5.1.0-beta1";

          src = ./.;

          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config makeWrapper ];
          buildInputs = with pkgs;
            [ ncurses ]
            ++ pkgs.lib.optionals x11 [ pkgs.libx11 ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DSUPPORT_X11_FRONTEND=${if x11 then "ON" else "OFF"}"
          ];

          installPhase = ''
            mkdir -p $out/bin $out/share/sil-q
            cp sil $out/bin/
            cp -a $src/lib/* $out/share/sil-q/
            wrapProgram $out/bin/sil --set ANGBAND_PATH $out/share/sil-q
          '';
        };
    in
    {
      packages.${system} = {
        default = self.packages.${system}.sil-q;
        sil-q = mkSilQ {};
        sil-q-nox = mkSilQ { x11 = false; };
      };

      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [ cmake ninja pkg-config gcc ncurses libx11 ];
      };
    };
}
