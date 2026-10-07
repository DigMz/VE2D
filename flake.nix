{
  description = "C++ Dev Env for Vulkan";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs }: {
    devShells.x86_64-linux.default =
      let
        pkgs = nixpkgs.legacyPackages.x86_64-linux;

        # nixpkgs ships imgui 1.91.x, but src/editor/editor_overlay.cpp targets the
        # 1.92.x Vulkan backend API (PipelineInfoMain, backend-managed font textures,
        # separate SAMPLED_IMAGE/SAMPLER descriptors), so pin imgui to match.
        imgui = (pkgs.imgui.override {
          IMGUI_BUILD_SDL3_BINDING = true;
          IMGUI_BUILD_VULKAN_BINDING = true;
        }).overrideAttrs (finalAttrs: _: {
          version = "1.92.9b";
          src = pkgs.fetchFromGitHub {
            owner = "ocornut";
            repo = "imgui";
            tag = "v${finalAttrs.version}";
            hash = "sha256-IjW+qddzKu9jOj3QCGhkChVK2UOvwl493ffUIIn/ZVQ=";
          };
        });
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
          imgui
          nlohmann_json
        ];

        shellHook = ''
          export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
        '';
      };
  };
}
