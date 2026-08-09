{
  description = "ESP32-WROOM-32 (ESP-IDF) development environment";

  inputs = {
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
          espShell = nixpkgs-esp-dev.devShells.${system}.esp32-idf;
        in
        {
          default = pkgs.mkShell {
            name = "esp32-wroom-32-dev";

            inputsFrom = [ espShell ];

            buildInputs = with pkgs; [
              picocom
              minicom 
              python3Packages.pyserial
              python3Packages.protobuf
            ];

            shellHook = ''
              echo "ESP32-WROOM-32 / ESP-IDF dev shell"
              echo "  idf.py --version"
              echo "  idf.py set-target esp32"
              echo "  idf.py menuconfig"
              echo "  idf.py build flash monitor"
            '';
          };
        });
    };
}
