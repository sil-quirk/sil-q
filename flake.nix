# Build sil-q with Nix.
#
# This flake provides reproducible builds of sil-q using cmake + ninja.
# It is intentionally scoped to proving the build works — not packaging for
# distribution. Distro packaging (install locations, desktop integration,
# runtime path wrapping, game data management) belongs in downstream package
# definitions like nixpkgs' package.nix.
#
# The mkSilQ function parameterizes the X11 frontend. Additional cmake flags
# can be threaded in as needed. Named packages cover the standard Linux build
# configurations; others can be built by calling mkSilQ directly from a
# downstream flake.
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

          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
          buildInputs = with pkgs;
            [ ncurses ]
            ++ pkgs.lib.optionals x11 [ pkgs.libx11 ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DSUPPORT_X11_FRONTEND=${if x11 then "ON" else "OFF"}"
          ];

          installPhase = ''
            mkdir -p $out/bin
            cp sil $out/bin/
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
