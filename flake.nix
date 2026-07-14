{
  description = "C++ Dev Env for Vulkan";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs }: {
    devShells.x86_64-linux.default =
      let
        pkgs = nixpkgs.legacyPackages.x86_64-linux;
      in
      pkgs.mkShell {
        packages = with pkgs; [
          gcc
          cmake
          ninja
          gnumake

          vulkan-headers
          vulkan-loader
          vulkan-validation-layers
          vulkan-tools
          vulkan-tools-lunarg
          sdl3
          glm
          tinyobjloader
          tinygltf
          ktx-tools
          stb
          shader-slang
        ];

        shellHook = ''
          export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
        '';
      };
  };
}
