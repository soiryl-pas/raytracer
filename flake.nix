{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-26.05";

  outputs = {self, nixpkgs}: let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };

    raytracer-base = stdenv: preset: stdenv.mkDerivation {
      name = "raytracer";
      version = "0.0.2";
      src = ./.;

      nativeBuildInputs = with pkgs; [
        cmake
        ninja
      ];
      buildInputs = with pkgs; [
        zlib
      ];

      configurePhase = ''
        cmake --preset ${preset}
      '';
      buildPhase = ''
        cmake --build build/${preset} -v -j
      '';
      installPhase = ''
        mkdir -p $out/bin
        cp ./build/${preset}/raytracer $out/bin
      '';

      env.NIX_ENFORCE_NO_NATIVE = 0;
    };

    raytracer = raytracer-base pkgs.stdenv "debug";
    raytracer-fast = raytracer-base pkgs.stdenv "fast";
    raytracer-clang = raytracer-base pkgs.clangStdenv "fast-without-lto";
  in {
    packages.${system} = {
      default = raytracer;
      fast = raytracer-fast;
      clang = raytracer-clang;
    };
    apps.${system}.default = {
      type = "app";
      program = "${raytracer}/bin/raytracer";
    };

    devShells.${system}.default = pkgs.mkShell {
      inputsFrom = [ raytracer ];

      packages = with pkgs; [
        clang-tools
        cppcheck
        gef
        bashInteractive
      ];

      env.NIX_ENFORCE_NO_NATIVE = 0;
    };
  };
}
