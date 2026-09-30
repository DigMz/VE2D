# VE2D

A 2D game engine written in C++20, built directly on Vulkan. Heavily a work in progress.

## Features

- **Vulkan 1.4 renderer** using modern dynamic rendering (no `VkRenderPass`/`VkFramebuffer`) and `synchronization2` barriers, built entirely on `vk::raii` handles.
- **Instanced sprite rendering**: every sprite is a quad drawn via a single instanced draw call, with per-instance transform/color/texture data supplied through a GPU storage buffer.
- **Runtime texture array**: textures are loaded lazily at runtime and indexed through a single combined-image-sampler descriptor array (up to 512 textures), so new textures can be registered without rebuilding the pipeline.
- **Scene graph**: a `Node` → `Node2D` → `Sprite` hierarchy with a Godot-style `init`/`ready`/`process` lifecycle, and parent-relative 2D transform propagation (position, rotation, scale) computed per frame.
- **Slang shaders**: shader source is written once in [Slang](https://shader-slang.com/) and compiled to SPIR-V at build time.
- **ImGui editor mode**: press `F1` in-app to switch to a Godot-style editor layout: a side panel on the left (an "Add Quad" button plus frame time, FPS, quad count, and texture count) and the game, rendered offscreen, in a "Game" panel on the right.

## Dependencies

- SDL3
- glm
- Vulkan SDK ≥ 1.4.335 (including `slangc`)
- tinyobjloader
- Dear ImGui (built with SDL3 + Vulkan backends)
- CMake ≥ 3.29, Ninja

All of the above are provided by the project's Nix flake.

## Build & Run

### Using Nix (recommended)

```sh
direnv allow      # or: nix develop
make conf_debug
make compile
make run
```

Or in one step: `make test` (configures, builds, and runs).

### Manual CMake

Make sure the dependencies above are installed and discoverable, then:

```sh
cmake -B build -G Ninja -DENABLE_CPP20_MODULE=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
build/VE2D/VE2D
```

## Controls

- `W`/`A`/`S`/`D` — move the camera up/left/down/right
- Middle mouse drag — pan the camera
- Scroll wheel / `Space` / `Ctrl` — move the camera forward/back along z
- In editor mode, the camera controls only respond while the mouse is over the "Game" panel
- `Space` / `Ctrl` — move up / down
- `Q` — add a quad (same as the editor's "Add Quad" button)
- `F1` — toggle editor mode (releases mouse capture so you can interact with it)
- `Escape` — leave editor mode, or toggle mouse capture if it's already closed

## Project Layout

- `src/app` — window creation, Vulkan instance/device/swapchain setup, and the main loop.
- `src/renderer` — the Vulkan rendering pipeline: pipeline/descriptor setup, buffers, textures, and per-frame command recording.
- `src/scenes` — bridges the node tree to the renderer, pushing sprite transforms and textures into GPU-side data each frame.
- `src/nodes` — the `Node`/`Node2D`/`Sprite` scene graph and transform propagation.
- `src/debug` — the ImGui-based editor UI.
- `src/utils` — shared Vulkan and general-purpose helper functions.
- `src/shaders` — Slang shader source, compiled to SPIR-V at build time.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for a deeper look at the rendering pipeline, and [`CLAUDE.md`](CLAUDE.md) for a denser architecture map geared toward working in this codebase.
