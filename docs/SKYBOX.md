# Cubemap skybox

The skybox is shared by gameplay, title and settings screens. Menus use a slow rotation; gameplay uses the current camera orientation. Camera translation cancels when reconstructing a ray between the near and far clip planes, so the sky stays at infinity. The orthographic overview uses parallel rays.

Files:
- `src/graphics/Skybox.h` and `.cpp`: cross-image validation, six-face upload, drawing and OpenGL state restoration.
- `assets/shaders/skybox.vert` and `.frag`: fullscreen triangle, inverse view-projection and `samplerCube` lookup.
- `src/graphics/RelayRenderer.*`: ownership, menu projection and scene integration. Statistics include the skybox draw and triangle.
- `src/main.cpp`: routes title/settings backgrounds to the skybox.
- `src/app/RelayMenu.cpp`: translucent full-window tint and local readable panels.
- `assets/textures/skybox/cross.jpg`: original user-supplied cross; see its adjacent README for attribution status.

Drawing order is clear, skybox, existing opaque/transparent scene, then UI. The skybox does not write depth, and restores the caller's depth/culling/blending, shader, VAO and texture state. It does not supply ambient illumination, change voxel storage, or implement ray tracing.

Build with `cmake -S . -B build` and `cmake --build build`; run `build/CoreExplorerLite.exe` on Windows or `build/CoreExplorerLite` on Linux. Check the title, settings, both levels, mouse orbit, pause/resume and restart. Menu backgrounds should show the sky around the local panels. Geometry and HUD must remain in front. The gameplay camera can look upward to inspect the sky. The bundled source is low resolution; blur/detail limits are asset limits.

When benchmarking the final game, include skybox rendering in the measured scene boundary and use identical sky settings in both baseline and optimised runs. No benchmark integration is added by this change.
