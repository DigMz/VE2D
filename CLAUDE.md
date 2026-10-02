# VE2D

A 2D game engine written in C++20, built directly on Vulkan (via `vk::raii`), using SDL3 for windowing/input, glm for math, Slang for shaders, and Dear ImGui for an in-app editor overlay. Rendering uses Vulkan 1.4 dynamic rendering and `synchronization2` — there is no `VkRenderPass`/`VkFramebuffer` anywhere in the codebase.

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

Ownership chain: `Application` owns a `Scene`, a `EditorOverlay`, an `InputHandler` and the `Camera`; `Scene` owns a `Renderer` and the root of a `Node` tree.

```
Application
 ├─ EditorOverlay
 ├─ InputHandler
 ├─ Camera
 └─ Scene
     ├─ Renderer
     └─ Node (root)
          └─ Node2D
               └─ Sprite
```

- **`Application`** (`src/app/app.hpp`, `src/app/app.cpp`) — SDL window creation, full Vulkan bring-up (instance, physical/logical device, swapchain, depth resources, command buffers, sync objects, camera UBOs), the main loop (event polling, per-frame `drawFrame()`), and swapchain recreation on resize. It acts on `InputActions` and `EditorActions` but does not interpret mouse or keyboard input itself.
- **`Scene`** (`src/scenes/scene.hpp`, `src/scenes/scene.cpp`) — the glue between the node tree and the `Renderer`. Walks the tree to register/update `Sprite`s (`pushSpriteDataToRenderer`/`updateSpriteDataOnRenderer`), recomputes global 2D transforms once per frame, and forwards `recordFrame()` to the `Renderer`. Exposes its root `Node` via `getRoot()`, which `Application` feeds to `EditorOverlay::buildUI()` for the Hierarchy panel.
- **`Node` / `Node2D` / `Sprite`** (`src/nodes/`) — a Godot-style scene graph. `Node` provides the `init`/`ready`/`process` lifecycle (each has a public recursive entry point and a `_init`/`_ready`/`_process` hook for subclasses to override). `Node2D` adds local transform state (position/rotation/scale) and computes parent-relative global transforms via `updateNode2DTransforms()`, which is invoked explicitly by `Scene` rather than as part of the `Node` lifecycle; `global_position`/`global_scale`/`global_rotation` are `protected`, so `Sprite` can read them directly. `Sprite` is the renderable leaf, holding a texture path and an index into the renderer's per-instance object list; it marks itself `dirty` by diffing its own cached **global** transform frame-to-frame (not just its local fields), so it also re-pushes to the renderer when an ancestor's transform changes it, with up to one frame of lag — see `docs/ARCHITECTURE.md`.
- **`Renderer`** (`src/renderer/renderer.hpp`, `src/renderer/renderer.cpp`) — owns the graphics pipeline, descriptor sets/layout, the shared unit-quad vertex/index buffers, the per-instance object storage buffer, and the texture array (images/views/samplers). Exposes `addQuadObject`/`updateQuadObject`/`addTexture` for `Scene` to drive, and `recordFrame()` to record the actual draw commands (including an optional callback for the editor overlay to draw into the same render pass). Also owns an offscreen "viewport" color/depth target per frame in flight (`resizeViewport`), which the scene is drawn into instead of the swapchain when the editor is open.
- **`EditorOverlay`** (`src/editor/editor_overlay.hpp`, `src/editor/editor_overlay.cpp`) — owns the entire Dear ImGui lifecycle (context, SDL3 backend, Vulkan backend) in isolation, so no other file needs to `#include` ImGui. In editor mode (`F1`) it lays out a left-hand panel (Add Quad button, Stats, a Selected Node inspector showing/editing name and local position/rotation/scale plus read-only global position/rotation, and a scrollable Hierarchy tree of the `Scene`'s `Node` tree with click-to-select) and a right-hand "Game" panel that displays the `Renderer`'s offscreen viewport image. User actions — including the clicked `selectedNode` — come back to `Application` as an `EditorActions` struct. `Renderer` invokes it only through an optional `std::function` callback.
- **`InputHandler`** (`src/input/input_handler.hpp`, `src/input/input_handler.cpp`) — mouse and keyboard game controls, separate from ImGui input (which `EditorOverlay` handles). Moves the `Camera` (WASD/Space/Ctrl held keys, middle-drag grab-pan, scroll-wheel zoom along z), owns mouse capture, and reports key shortcuts (`F1`, `Escape` in the editor, `Q`) to `Application` as an `InputActions` struct. `Application` tells it the game view's size and hover state each frame (`setGameView`), so camera controls only respond over the editor's "Game" panel.
- **`Camera`** (`src/input/camera.hpp`) — plain camera state (position/front/up/fov). `Application` builds the camera UBO from it.
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
