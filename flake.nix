{
  description = "C project";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "minnie16";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.pkg-config
          ];
          buildInputs = [ ];

        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            gcc
            gdb
            cmake
            gnumake
            pkg-config
            clang-tools
            # valgrind
          ];
        };
      }
    );
}
