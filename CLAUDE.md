# VE2D

A 2D game engine written in C++20, built directly on Vulkan (via `vk::raii`), using SDL3 for windowing/input, glm for math, Slang for shaders, and Dear ImGui for an in-app debug overlay. Rendering uses Vulkan 1.4 dynamic rendering and `synchronization2` — there is no `VkRenderPass`/`VkFramebuffer` anywhere in the codebase.

## Build Commands

The Nix flake (`flake.nix`, auto-loaded via `.envrc`/direnv) provides every dependency, including `slangc`. Either `direnv allow` or `nix develop` first.

Via the `Makefile`:

```sh
make conf_debug   # configure (Debug) with CMake+Ninja
make conf_release # configure (Release)
make compile      # cmake --build build
make run          # runs build/VE2D/VE2D
make test         # compile + run
make clean        # rm -rf build
```

Equivalent raw CMake:

```sh
cmake -B build -G Ninja -DENABLE_CPP20_MODULE=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
build/VE2D/VE2D
```

`compile_commands.json` at the repo root is a symlink to `build/compile_commands.json`, created by `make configure`, for clangd/IDE tooling.

## Architecture Map

Ownership chain: `Application` owns a `Scene` and a `DebugOverlay`; `Scene` owns a `Renderer` and the root of a `Node` tree.

```
Application
 ├─ DebugOverlay
 └─ Scene
     ├─ Renderer
     └─ Node (root)
          └─ Node2D
               └─ Sprite
```

- **`Application`** (`src/app/app.hpp`, `src/app/app.cpp`) — SDL window creation, full Vulkan bring-up (instance, physical/logical device, swapchain, depth resources, command buffers, sync objects, camera UBOs), the main loop (input, per-frame `drawFrame()`), and swapchain recreation on resize.
- **`Scene`** (`src/scenes/scene.hpp`, `src/scenes/scene.cpp`) — the glue between the node tree and the `Renderer`. Walks the tree to register/update `Sprite`s (`pushSpriteDataToRenderer`/`updateSpriteDataOnRenderer`), recomputes global 2D transforms once per frame, and forwards `recordFrame()` to the `Renderer`.
- **`Node` / `Node2D` / `Sprite`** (`src/nodes/`) — a Godot-style scene graph. `Node` provides the `init`/`ready`/`process` lifecycle (each has a public recursive entry point and a `_init`/`_ready`/`_process` hook for subclasses to override). `Node2D` adds local transform state (position/rotation/scale) and computes parent-relative global transforms via `updateNode2DTransforms()`, which is invoked explicitly by `Scene` rather than as part of the `Node` lifecycle. `Sprite` is the renderable leaf, holding a texture path and an index into the renderer's per-instance object list.
- **`Renderer`** (`src/renderer/renderer.hpp`, `src/renderer/renderer.cpp`) — owns the graphics pipeline, descriptor sets/layout, the shared unit-quad vertex/index buffers, the per-instance object storage buffer, and the texture array (images/views/samplers). Exposes `addQuadObject`/`updateQuadObject`/`addTexture` for `Scene` to drive, and `recordFrame()` to record the actual draw commands (including an optional callback for the debug overlay to draw into the same render pass).
- **`DebugOverlay`** (`src/debug/debug_overlay.hpp`, `src/debug/debug_overlay.cpp`) — owns the entire Dear ImGui lifecycle (context, SDL3 backend, Vulkan backend) in isolation, so no other file needs to `#include` ImGui. `Renderer` invokes it only through an optional `std::function` callback.
- **`src/utils/vulkan_utils.hpp`** — shared Vulkan helpers (buffer/image creation, memory type lookup, layout transitions, one-shot command buffers) and the core data structs used across the pipeline: `Vertex`, `QuadObject` (CPU-side per-sprite data), `CameraUBO`, `GPUObject` (GPU-side per-instance data).
- **`src/utils/utils.hpp`** — general helpers: `readFile` (binary file loading, used for SPIR-V) and 2D vector rotation helpers.
- **`src/shaders/shader.slang`** — single Slang source with `vertMain`/`fragMain` entry points, compiled by CMake via `slangc` into `src/shaders/slang.spv`.

## Rendering Model

There is one hardcoded unit quad shared by every sprite. Per-sprite variation (position, rotation, scale, color, texture) comes entirely from instanced rendering: `Scene` maintains a `QuadObject` per `Sprite`, `Renderer` converts these into `GPUObject`s (with a CPU-computed model matrix) each frame and uploads them to a storage buffer, and a single `drawIndexed(6, instanceCount, ...)` call renders every sprite in one draw. The fragment shader indexes into a texture array (bound as a single combined-image-sampler descriptor with `updateAfterBind`) using a per-instance texture index, and tints the sampled color using a per-instance color while keeping the sampled alpha for blending.

## Conventions

- C++20, `vk::raii` wrappers throughout — no manual `Vk*` handle management or `vk::UniqueHandle`.
- Dynamic rendering (`vk::RenderingInfo`/`beginRendering`/`endRendering`), not `VkRenderPass`/`VkFramebuffer`.
- `synchronization2` barriers (`vk::ImageMemoryBarrier2`/`pipelineBarrier2`) for image layout transitions during frame recording.
- Shaders are written in Slang, not GLSL/HLSL, and compiled with `slangc` at build time via a CMake custom command.
- An optional C++20-modules path for Vulkan exists (`ENABLE_CPP20_MODULE` CMake option, off by default) — the default build path uses plain `#include <vulkan/vulkan_raii.hpp>`.

For more detail on the rendering pipeline specifically (Vulkan setup, frame lifecycle, descriptor layout, shader I/O, transform propagation), see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).
