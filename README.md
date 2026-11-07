# Core Explorer Lite

## Download and play (Windows x64)

Download **Core-Explorer-Lite-Windows-x64.zip** from [GitHub Releases](https://github.com/JerryX411/COMP9415-Group-Project/releases/tag/lite-2026-11-07). Extract the whole ZIP, then double-click **Play.cmd**. No compiler, CMake, Python or build step is required. Keep the assets folder beside the executable. Requires 64-bit Windows and an OpenGL 3.3-capable graphics driver. GitHub's Code > Download ZIP is source code, not this ready-to-play package.


The linked release is the November 7 snapshot; newer local graphics demonstrations require rebuilding this source.

Current Member-A version: the reduced edition of Core Explorer. The advanced version is preserved in Git history (48f981d) and the separate Core-Explorer repository.

Includes two simple voxel yards, robot movement/animation, WASD, mouse camera, Space jump, C toggle crouch, E switch interaction, retractable spikes, moving blocks, armour, restart/pause, and a plain UI over a cubemap background. E retracts the linked spikes for five seconds. R restarts; Esc pauses. Select LEVEL in the title menu to change maps.

Removed: arrival cinematic, sprint/boost/slide/wall actions, jetpack, portals, checkpoints, advanced pickup effects, training, workshop, advanced particle effects, and the full audio pack. The reduced build is intentionally silent.

Build: `cmake -S . -B build`, then `cmake --build build`. Run `build/CoreExplorerLite.exe` (Windows) or `build/CoreExplorerLite` (Linux). Tests: `ctest --test-dir build --output-on-failure`.

Title, pause, settings and result screens use large bold text over the scene: no panels, borders, button fills or fullscreen tint. Selected menu text is cyan; the gameplay HUD mirrors the advanced layout: five slanted armour cells at top left, filled/dim hexagonal core slots at bottom centre, and large countdown digits (or an infinity glyph in training) at top right, all without backing panels or outlines. A small glyph shadow preserves readability.

UI uses Segoe UI Bold from Windows when available, or the bundled Rajdhani font under its included OFL licence. Robot and map geometry are generated in code.

## Coursework reference

The original assignment requirements are retained in [docs/ASSIGNMENT.md](docs/ASSIGNMENT.md). ProjectPlan.pdf is the previously submitted plan for the earlier version; it has not been rewritten to describe this reduced build. This build now includes a core-collection objective and exit completion; a final report is still not included, and this is not a claim of full assignment compliance.

## Core collection tutorial

Each map has three blue cores. Approach and press the interact key (E by default); collecting all three opens the exit at the far-right end of the yard. Walk through the door to complete the level, then Enter selects the next map or R restarts. Progress and context hints appear in the plain HUD. Pickup uses a short reach animation and disappears; the exit indicator turns green and the two panels slide apart. No explosive particles, cinematic, or pickup camera shake is added. Reset clears collected cores and closes the gate.

## Member-B integration

Integrated JerryX411's skybox contribution from Member-B commit db8a1acf1a2562a64f99932586163ae0706976a1 using an author-preserving cherry-pick. Includes cubemap rendering, menu panels, surface detail adjustments and the tutorial passage change. The preceding a08ef51 commit only packages build tools/dependencies and generated output; it is intentionally excluded from the source checkout. See docs/SKYBOX.md for implementation and assets/textures/skybox/README.md for the supplied image's attribution status.

## Lite timer and impact feedback

The Tutorial Yard has unlimited time (an infinity glyph). The Obstacle Yard starts with a 120-second countdown, displayed as minutes:seconds and highlighted during the last 20 seconds. Timing pauses with the pause/settings menu or loss of focus and freezes on completion/failure. At zero, TIME UP ends the challenge without applying damage. Restart or level change restores the appropriate limit. Result panels show remaining time or UNLIMITED.

Damage briefly recoils the torso/head/arms in the hit direction and emits 24 small sparks/metal fragments. The fixed 128-particle pool uses gravity, drag, voxel, moving-obstacle and fixture collision/bounce and lifetime cleanup, including after failure; pausing freezes the simulation. No continuous emitter is active. The supplied skybox retains its original image, with a gentle blue lift, horizon haze, mip filtering and slower menu rotation.

Core collection emits one short burst of 20 cyan-white sparks; impact sparks are amber with slightly longer luminous streaks. Both reuse the bounded particle pool and expire automatically.

## Jump pads

Each yard has one flush cyan chevron pad (tutorial: 10,1,19; obstacle: 26,1,12). Step or land on it to launch automatically about 4.3 cells above the floor, versus roughly 1.8 for a held normal jump. No Space press/hold is needed; steer in the air with WASD. A cyan-white upward plume and a 0.4-second trail beneath the ascending robot mark launch; particles expire within 0.72 seconds. Landing on it bounces again, while passing above it does not trigger it. Normal ceiling and wall collisions still apply.

## Graphics demonstration (Normal Mapping + Particles)

Normal mapping is enabled by default. During play: **F1** toggles diagnostics, **F2** toggles normal mapping, **F3** shows OFF on the left and ON on the right, **F4** freezes/unfreezes simulation, **F5** cycles normal strength (0 to 2), **F6** toggles hidden-face removal for the core optimisation comparison. These keys are reserved from gameplay rebinding. Diagnostics show camera position, FOV, geometry/draw counts and live particle occupancy. The normal HUD remains frameless and unchanged with diagnostics closed.

See [docs/CHALLENGES.md](docs/CHALLENGES.md) for implementation, reproducible demonstrations and the exact benchmark procedure. No third challenge is claimed: hidden-face removal is the core optimisation. Linux/VLAB verification and the final assessed report remain outstanding.
