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
      forSystem =
        system:
        let
          pkgs = import nixpkgs {
            inherit system;
            overlays = [ localOverlay ];
          };
          serve = pkgs.writeShellApplication {
            name = "docs-serve";
            runtimeInputs = [ pkgs.mdbook ];
            text = ''exec mdbook serve "$@"'';
          };
        in
        {
          legacyPackages = pkgs;
          packages = {
            inherit (pkgs) docs;
            default = pkgs.docs;
            inherit serve;
          };
          apps.serve = utils.lib.mkApp { drv = serve; };
          apps.default = utils.lib.mkApp { drv = serve; };
          devShells.default = pkgs.mkShell {
            packages = with pkgs; [
              mdbook
              presenterm
            ];
          };
        };
    in
    utils.lib.eachSystem systems forSystem // { overlays.default = localOverlay; };
}
