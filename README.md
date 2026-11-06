# Core Explorer Lite

Current Member-A version: the reduced edition of Core Explorer. The advanced version is preserved in Git history (48f981d) and the separate Core-Explorer repository.

Includes two simple voxel yards, robot movement/animation, WASD, mouse camera, Space jump, C toggle crouch, E switch interaction, retractable spikes, moving blocks, armour, restart/pause, and a plain UI over a cubemap background. E retracts the linked spikes for five seconds. R restarts; Esc pauses. Select LEVEL in the title menu to change maps.

Removed: arrival cinematic, sprint/boost/slide/wall actions, jetpack, portals, checkpoints, advanced pickup effects, training, workshop, advanced particle effects, and the full audio pack. The reduced build is intentionally silent.

Build: `cmake -S . -B build`, then `cmake --build build`. Run `build/CoreExplorerLite.exe` (Windows) or `build/CoreExplorerLite` (Linux). Tests: `ctest --test-dir build --output-on-failure`.

Title, pause, settings, HUD and result screens use large bold text only over the scene: no panels, borders, button fills or fullscreen tint. Selected menu text is cyan; the gameplay HUD mirrors the advanced layout: five slanted armour cells at top left, filled/dim hexagonal core slots at bottom centre, and large countdown digits (or an infinity glyph in training) at top right, all without backing panels or outlines. A small glyph shadow preserves readability.

UI uses Segoe UI Bold from Windows when available, or the bundled Rajdhani font under its included OFL licence. Robot and map geometry are generated in code.

## Coursework reference

The original assignment requirements are retained in [docs/ASSIGNMENT.md](docs/ASSIGNMENT.md). ProjectPlan.pdf is the previously submitted plan for the earlier version; it has not been rewritten to describe this reduced build. This build now includes a core-collection objective and exit completion; a final report is still not included, and this is not a claim of full assignment compliance.

## Core collection tutorial

Each map has three blue cores. Approach and press the interact key (E by default); collecting all three opens the exit at the far-right end of the yard. Walk through the door to complete the level, then Enter selects the next map or R restarts. Progress and context hints appear in the plain HUD. Pickup uses a short reach animation and disappears; the exit indicator turns green and the two panels slide apart. No explosive particles, cinematic, or pickup camera shake is added. Reset clears collected cores and closes the gate.

## Member-B integration

Integrated JerryX411's skybox contribution from Member-B commit db8a1acf1a2562a64f99932586163ae0706976a1 using an author-preserving cherry-pick. Includes cubemap rendering, menu panels, surface detail adjustments and the tutorial passage change. The preceding a08ef51 commit only packages build tools/dependencies and generated output; it is intentionally excluded from the source checkout. See docs/SKYBOX.md for implementation and assets/textures/skybox/README.md for the supplied image's attribution status.

## Lite timer and impact feedback

The Tutorial Yard has unlimited time (TIME UNLIMITED). The Obstacle Yard starts with a 120-second countdown, displayed as minutes:seconds and highlighted during the last 20 seconds. Timing pauses with the pause/settings menu or loss of focus and freezes on completion/failure. At zero, TIME UP ends the challenge without applying damage. Restart or level change restores the appropriate limit. Result panels show remaining time or UNLIMITED.

Damage briefly recoils the torso/head/arms in the hit direction and emits 24 small sparks/metal fragments. The fixed 64-particle pool uses gravity, drag, voxel collision/bounce and lifetime cleanup, including after failure; pausing freezes the simulation. No continuous emitter is active. The supplied skybox retains its original image, with a gentle blue lift, horizon haze, mip filtering and slower menu rotation.

Core collection emits one short burst of 20 cyan-white sparks; impact sparks are amber with slightly longer luminous streaks. Both reuse the bounded particle pool and expire automatically.
