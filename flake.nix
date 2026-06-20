{
  description = "Description for the project";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    git-hooks = {
      url = "github:cachix/git-hooks.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [ inputs.git-hooks.flakeModule ];
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
        "x86_64-darwin"
      ];
      perSystem =
        {
          config,
          pkgs,
          ...
        }:
        {
          pre-commit.settings.hooks = {
            nixfmt.enable = true;
            yamlfmt.enable = true;
            yamllint.enable = true;
            clang-format = {
              enable = true;
              types_or = [
                "c"
                "c++"
              ];
            };
          };

          devShells = {
            default =
              let
                __zed =
                  let
                    defaults = {
                      language_servers = [ "clangd" ];
                      formatter = "language_server";
                      format_on_save = "on";
                    };
                  in
                  pkgs.writers.writeJSON "settings.json" {
                    lsp.clangd.binary = {
                      path = "${pkgs.clang-tools}/bin/clangd";
                      # Let clangd query the Nix compiler wrapper for its system
                      # include paths (libstdc++, glibc, etc.), which Nix injects
                      # via the environment rather than into compile_commands.json.
                      arguments = [ "--query-driver=**/bin/clang*" ];
                    };

                    languages.C = defaults;
                    languages."C++" = defaults;
                  };
              in
              pkgs.mkShell {
                packages =
                  with pkgs;
                  [
                    nil
                    nixd
                    nixfmt
                    go-task
                    clang
                    clang-tools
                    cmake
                    ninja
                    pkg-config
                    gtest
                  ]
                  ++ config.pre-commit.settings.enabledPackages;

                shellHook = ''
                  export CC=clang
                  export CXX=clang++
                  mkdir -p .zed
                  ln -sf ${__zed} .zed/settings.json
                  ${config.pre-commit.shellHook}
                '';
              };

            ci = pkgs.mkShell {
              packages =
                with pkgs;
                [
                  go-task
                  clang
                  clang-tools
                  cmake
                  ninja
                  pkg-config
                  gtest
                ]
                ++ config.pre-commit.settings.enabledPackages;
              shellHook = ''
                export CC=clang
                export CXX=clang++
                ${config.pre-commit.shellHook}
              '';
            };
          };
        };
    };
}
