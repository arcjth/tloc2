{
  description = "esp32devenv";

  inputs = {
    # Unpin from the broken commit and track the repository default
    nixpkgs-esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
  };

  outputs = { self, nixpkgs-esp-dev }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      nixpkgs = nixpkgs-esp-dev.inputs.nixpkgs;
      forEachSystem = f: nixpkgs.lib.genAttrs systems f;
    in
    {
      devShells = forEachSystem (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          # Replace deprecated esp32-idf shell with esp-idf-full (or esp-idf-xtensa)
          espShell = nixpkgs-esp-dev.devShells.${system}.esp-idf-full;
        in
        {
          default = pkgs.mkShell {
            name = "esp32devenv";

            inputsFrom = [ espShell ];

            buildInputs = with pkgs; [
              picocom
              minicom 
              python3Packages.matplotlib
              python3Packages.pyserial
              python3Packages.rich
              python3Packages.markdown
              python3Packages.protobuf
            ];

            shellHook = ''
              echo "ESP32-WROOM-32 / ESP-IDF devshell"
              echo "  idf.py --version"
              echo "  idf.py set-target esp32"
              echo "  idf.py menuconfig"
              echo "  idf.py build flash monitor"
            '';
          };
        });
    };
}
