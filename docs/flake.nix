{
  description = "Minnie16 Docs";

  inputs = {
    utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  };

  outputs =
    {
      self,
      nixpkgs,
      utils,
    }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      localOverlay = import ./nix/overlay.nix;
      forSystem = system: rec {
        legacyPackages = import nixpkgs {
          inherit system;
          overlays = [ localOverlay ];
        };
        packages = utils.lib.flattenTree {
          inherit (legacyPackages) devShell docs;
        };
        defaultPackage = packages.docs;
        apps.docs = utils.lib.mkApp { drv = packages.docs; };
      };
    in
    utils.lib.eachSystem systems forSystem // { overlay = localOverlay; };
}
