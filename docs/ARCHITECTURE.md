# VE2D Rendering Architecture

This document covers the rendering pipeline in detail: Vulkan setup, the per-frame lifecycle, how sprite data flows to the GPU, the descriptor/pipeline layout, texture handling, shader I/O, and 2D transform propagation.

## Vulkan Setup

All Vulkan setup is driven by `Application` (`src/app/app.cpp`), using `vk::raii` handles throughout.

- **Instance**: `createInstance()` builds a `vk::ApplicationInfo` targeting API version 1.4, checks for the required SDL Vulkan extensions (plus `VK_EXT_debug_utils` and the `VK_LAYER_KHRONOS_validation` layer in debug builds), and creates the `vk::raii::Instance`.
- **Debug messenger**: `setupDebugMessenger()` registers a `DebugUtilsMessengerEXT` for validation/performance messages when validation layers are enabled.
- **Surface**: `createSurface()` wraps `SDL_Vulkan_CreateSurface` in a `vk::raii::SurfaceKHR`.
- **Physical device selection**: `pickPhysicalDevice()` scores available GPUs (discrete > integrated > virtual > CPU) via a `std::multimap`, requiring Vulkan ≥1.3, a graphics+present queue, and support for `dynamicRendering`, `synchronization2`, `extendedDynamicState`, `descriptorBindingVariableDescriptorCount`, `descriptorBindingSampledImageUpdateAfterBind`, and `shaderDrawParameters`.
- **Logical device**: `createLogicalDevice()` builds a `vk::StructureChain` of `PhysicalDeviceFeatures2` plus Vulkan 1.1/1.2/1.3 feature structs and `ExtendedDynamicStateFeaturesEXT`, enabling `samplerAnisotropy`, `shaderDrawParameters`, `descriptorBindingSampledImageUpdateAfterBind`, `descriptorBindingVariableDescriptorCount`, `runtimeDescriptorArray`, `synchronization2`, `dynamicRendering`, and `extendedDynamicState`. A single `vk::raii::Queue` handles both graphics and present.
- **Swapchain**: `createSwapChain()` requests at least 3 images, prefers `B8G8R8A8Srgb`/`SrgbNonlinear` surface format and Mailbox present mode (falling back to FIFO), and sizes the extent from the window's pixel size.
- **Depth resources**: a single shared depth image/view sized to the swapchain extent; format chosen by `vk_util::findDepthFormat` (tries `D32Sfloat`, `D32SfloatS8Uint`, `D24UnormS8Uint` in order).
- **Sync objects**: `MAX_FRAMES_IN_FLIGHT = 2`. One `renderFinishedSemaphore` per swapchain image; per-frame-in-flight `presentCompleteSemaphore` and a signaled `inFlightFence`.
- **Camera UBOs**: one persistently-mapped, host-visible/coherent uniform buffer per frame-in-flight, sized `sizeof(CameraUBO)`.

## Frame Lifecycle

`Application::drawFrame()` runs once per iteration of the main loop:

1. Wait on `inFlightFences[frameIndex]` — CPU blocks until the GPU finished the frame from `MAX_FRAMES_IN_FLIGHT` iterations ago.
2. Acquire the next swapchain image (`acquireNextImage`), signaling `presentCompleteSemaphores[frameIndex]`. On `eErrorOutOfDateKHR`, calls `recreateSwapChain()` and skips the rest of the frame.
3. Recompute the camera's view/projection matrices (`glm::lookAt` / Y-flipped `glm::perspective`) and copy them into the current frame's camera UBO.
4. Reset the fence and the frame's command buffer.
5. Delegate to `Scene::recordFrame()` → `Renderer::recordFrame()` to record the actual draw commands (see below), passing an optional overlay-draw callback when the editor overlay is visible.
6. Submit the command buffer, waiting on `presentCompleteSemaphores[frameIndex]` at the color-attachment-output stage and signaling `renderFinishedSemaphores[imageIndex]`, fenced by `inFlightFences[frameIndex]`.
7. Present, waiting on `renderFinishedSemaphores[imageIndex]`; on suboptimal/out-of-date or a pending resize, calls `recreateSwapChain()`.
8. Advance `frameIndex` modulo `MAX_FRAMES_IN_FLIGHT`.

Note the semaphore indexing: `presentCompleteSemaphores`/`inFlightFences` are indexed by `frameIndex` (frame-in-flight slot), while `renderFinishedSemaphores` are indexed by `imageIndex` (swapchain image) — acquisition order doesn't map 1:1 to frame-in-flight slots, so these need separate indices.

### `Renderer::recordFrame()`

1. `updateGPUObjectsBuffer()` — rebuild and upload the current frame's per-instance data (see Data Flow below).
2. Begin the command buffer.
3. Scene pass (`beginPass()` + `drawScene()`):
   - `beginPass()` barriers the color target `eUndefined` → `eColorAttachmentOptimal` and the depth target `eUndefined` → `eDepthAttachmentOptimal` (via `pipelineBarrier2`), then calls `beginRendering()` with one color attachment (clear to black, store) and one depth attachment (clear to 1.0, don't-care store). This is dynamic rendering, with no render pass/framebuffer objects.
   - `drawScene()` binds the pipeline, sets the dynamic viewport/scissor to the target's extent, binds the shared vertex/index buffers and the current frame's descriptor set, then issues `drawIndexed(6, instanceCount, 0, 0, 0)`. That is one instanced draw for every sprite.
   - **Game mode** (`sceneToViewport = false`): the targets are the swapchain image and the app's depth image.
   - **Editor mode** (`sceneToViewport = true`): the targets are this frame-in-flight's offscreen viewport color/depth images (sized to the editor's "Game" panel). After `endRendering()`, the viewport color image is barriered to `eShaderReadOnlyOptimal`, and a second `beginPass()` opens on the swapchain image, which holds only the UI.
4. If an overlay-draw callback was supplied, invoke it inside the currently open rendering scope (the swapchain pass). In editor mode, ImGui samples the viewport image as a texture in its "Game" panel.
5. `endRendering()`.
6. Barrier the swapchain image `eColorAttachmentOptimal` → `ePresentSrcKHR`.
7. End the command buffer.

The viewport targets are created by `Renderer::resizeViewport()`, which waits for the device to go idle before reallocating. `Application` calls it after `drawFrame()` whenever the "Game" panel's pixel size changes, then re-registers the new image views with ImGui through `EditorOverlay::setViewportTextures()`. While the editor is open, the camera projection uses the viewport's aspect ratio instead of the swapchain's.

## Data Flow: Sprites to GPU

- `QuadObject` (`src/utils/vulkan_utils.hpp`) is the CPU-facing, per-sprite struct: position, rotation (Z-only), scale, color, texture index.
- `Scene::pushSpriteDataToRenderer()` (called once per `Sprite` during `Scene::init()`) and `Scene::updateSpriteDataOnRenderer()` (called each frame for sprites whose `dirty` flag is set) read a sprite's **global** transform and call `Renderer::addQuadObject()`/`updateQuadObject()` to register/update its `QuadObject`.
- `Node2D::transformDirty` is computed in `Node2D::updateTransform()` by diffing the node's **local** position/scale/rotation against last frame's cached values, OR'd with the parent's own `transformDirty` — since `updateTransform()` visits a parent before its children within a single `updateNode2DTransforms()` pass, a dirty parent is observed by its children the same frame, with no lag. `Sprite` overrides `updateTransform()` to fold `transformDirty` into its own `dirty` flag immediately (plus a separate diff on `texturePath` in `Sprite::_process`). `dirty` is cleared immediately after `Scene::process()` pushes the update.
- `Renderer::updateGPUObjects()` rebuilds a `std::vector<GPUObject>` from the current `QuadObject`s once per frame, computing a full model matrix (`translate * rotateZ * scale`) per object via glm.
- `GPUObject` (`src/utils/vulkan_utils.hpp`) is the GPU-facing struct — `model` (mat4), `color` (vec3), `textureIndex` — written into a storage buffer bound at descriptor binding 1.
- `Renderer::updateGPUObjectsBuffer()` copies the rebuilt array into the storage buffer each frame; if the required size exceeds current capacity, it grows the buffer (doubling), recreates it, and rewrites binding 1 in every per-frame descriptor set. Initial capacity is `sizeof(GPUObject) * 10000`.
- The vertex shader reads `SV_InstanceID` to index into this array per instance.

## Descriptor & Pipeline Layout

A single descriptor set layout with three bindings, all using `eUpdateAfterBindPool`:

| Binding | Type | Stage | Notes |
|---|---|---|---|
| 0 | Uniform buffer | Vertex | `CameraUBO` (view + projection matrices) |
| 1 | Storage buffer | Vertex | `GPUObject[]` — per-instance model matrix, color, texture index |
| 2 | Combined image sampler array | Fragment | `descriptorCount = MAX_TEXTURES` (512), `eVariableDescriptorCount \| eUpdateAfterBind` |

The `eUpdateAfterBind`/`eFreeDescriptorSet` descriptor pool flags are what allow `Renderer::addTexture()` to patch binding 2 of every per-frame descriptor set live, without invalidating in-flight command buffers.

Pipeline state: triangle list topology, dynamic viewport + scissor, back-face culling with counter-clockwise front face, depth test enabled (`eLess`) with depth **write disabled** (2D sprites don't need to occlude by depth), and standard alpha blending (`srcAlpha`/`oneMinusSrcAlpha`) to support transparent textures. Built with `vk::PipelineRenderingCreateInfo` chained onto the pipeline create info (dynamic rendering — no render pass object), using the swapchain's surface format for the color attachment and `vk_util::findDepthFormat` for the depth attachment.

## Textures

- `Renderer::createTextureImages()` loads images via `stb_image` (`stbi_load`, forced to RGBA8), uploads through a transient staging buffer, and creates a device-local `eR8G8B8A8Srgb` image, transitioning `eUndefined` → `eTransferDstOptimal` → `eShaderReadOnlyOptimal`.
- `Renderer::addTexture(texturePath)` is the runtime path: it lazily loads a new texture, appends it to the internal arrays, and patches descriptor binding 2 at the new array index in every per-frame descriptor set. `Scene` calls this the first time it encounters a given texture path, caching the resulting index.
- A single shared `vk::raii::Sampler` (linear filtering, repeat addressing, anisotropy enabled) is used for every texture — one combined-image-sampler array rather than a sampler per texture.

## Shader I/O (`src/shaders/shader.slang`)

Compiled by CMake via `slangc -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain` into `src/shaders/slang.spv`.

- `VSInput`: `inPosition` (float3), `inTexCoord` (float2) — matches the C++ `Vertex` struct and its binding/attribute descriptions.
- `UniformBuffer` (binding 0): `view`, `proj` (mat4x4) — matches `CameraUBO`. The model matrix is intentionally not here; it's per-instance.
- `ObjectData` (binding 1, `StructuredBuffer<ObjectData>`): `model` (mat4x4), `color` (float3), `textureIndex` (uint32) — matches `GPUObject`.
- Binding 2 hosts both `Texture2D textures[]` and `SamplerState samplerState` — Slang emits this as a single combined-image-sampler binding at the Vulkan level (intentional; the compiler's warning about the shared binding is explicitly suppressed).
- `vertMain`: reads `SV_InstanceID` to index into `objects[]`, computes `pos = proj * view * model * vec4(inPosition, 1)`, and passes the per-instance `color`/`textureIndex` plus the vertex's `texCoord` through to the fragment stage.
- `fragMain`: samples `textures[textureIndex]` at `fragTexCoord`, multiplies the sampled RGB by the per-instance `color` (tint), and keeps the sampled alpha — combined with the pipeline's alpha blending, this is how sprite tinting and transparency work.

## Transform Propagation

`Node2D` (`src/nodes/node2D.hpp`/`.cpp`) holds local `position` (vec3), `scale` (vec2), and `rotation` (a single angle), plus cached global equivalents.

`Node2D::updateTransform()`:

1. If the parent is also a `Node2D`:
   - `global_rotation = parent.global_rotation + rotation` — computed first, since the next step rotates by the accumulated global angle, not just this node's own local `rotation`.
   - The local `position` is rotated by that `global_rotation` (2D rotation applied to x/y; z untouched), then: `global_position = parent.global_position + (rotated local position, scaled by parent.global_scale, z left unscaled)`.
   - `global_scale = parent.global_scale * scale` (component-wise)
2. If there is no `Node2D` parent (root, or the parent is a plain `Node`), the globals are just set equal to the locals.
3. After updating itself, it recurses into its children via `updateNode2DTransforms()`, so children always see an already-updated parent global transform.

Rotating by the accumulated `global_rotation` (rather than the node's own local `rotation`) is what makes a child correctly orbit around an already-rotated parent instead of only spinning in place around its own local offset.

`updateNode2DTransforms(Node&)` walks a subtree looking for `Node2D` instances. If a node in the tree isn't itself a `Node2D`, the walk still recurses into its children, so non-`Node2D` intermediate nodes don't block propagation to `Node2D` descendants further down the tree.

Alongside the global transform, `updateTransform()` also maintains `transformDirty`: true when this node's local position/scale/rotation changed since last frame, or when the parent's `transformDirty` is true. See "Data Flow: Sprites to GPU" above for how `Sprite` consumes this.

This pass is driven explicitly by `Scene::init()` (once, after the tree's `init()` pass) and `Scene::process()` (once per frame, after the tree's `process()` pass) — it is a separate tree traversal from the `Node` lifecycle (`init`/`ready`/`process`), not part of it.
