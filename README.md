# Core Explorer Lite

## Download and play (Windows x64)

Download **Core-Explorer-Lite-Windows-x64.zip** from [GitHub Releases](https://github.com/JerryX411/COMP9415-Group-Project/releases/tag/lite-2026-11-07). Extract the whole ZIP, then double-click **Play.cmd**. No compiler, CMake, Python or build step is required. Keep the assets folder beside the executable. Requires 64-bit Windows and an OpenGL 3.3-capable graphics driver. GitHub's Code > Download ZIP is source code, not this ready-to-play package.


The linked release is the November 7 snapshot; newer local graphics demonstrations require rebuilding this source.

Current Member-A version: the reduced edition of Core Explorer. The advanced version is preserved in Git history (48f981d) and the separate Core-Explorer repository.

Includes two simple voxel yards plus Helios Yard (see below), robot movement/animation, WASD, Left Shift sprint, mouse camera, Space jump, C toggle crouch, E switch interaction, electric floor plates, moving blocks, armour, restart/pause, and a plain UI over the Member B cubemap sky (see [docs/SKYBOX.md](docs/SKYBOX.md)). E disables the linked floor plate for five seconds. R restarts; Esc pauses. Select LEVEL in the title menu to change maps.

The rotating minimap is visible by default in every yard. Press M to show or hide it for the current session. While moving, the courier faces the mouse camera's heading. When stopped, move the mouse to orbit around the courier.

Removed: arrival cinematic, boost/slide/wall actions, jetpack, checkpoints, advanced pickup effects, training, workshop, advanced particle effects, and the full audio pack. The reduced build is intentionally silent.

Build: `cmake -S . -B build`, then `cmake --build build`. Run `build/CoreExplorerLite.exe` (Windows) or `build/CoreExplorerLite` (Linux). Tests: `ctest --test-dir build --output-on-failure`.

Title, pause and settings use large text over translucent local panels, so the cubemap remains visible. The pause menu also dims the scene. Selected menu text is cyan; the gameplay HUD mirrors the advanced layout: five slanted armour cells at top left, filled/dim hexagonal core slots at bottom centre, and large countdown digits (or an infinity glyph in training) at top right, all without backing panels or outlines. A small glyph shadow preserves readability.

UI uses Segoe UI Bold from Windows when available, or the bundled Rajdhani font under its included OFL licence. Robot and map geometry are generated in code.

## Helios Yard (synced from the full edition)

The third level, **Helios Yard**, is the full edition's hand-edited Level 1. Choose it with LEVEL on the title screen (or `--level 2`); Enter after a win moves on through the three levels. The map is read from `assets/levels/helios_yard.txt`, a copy of the full edition's `level0.txt` in the same file format (`LevelData`). To update it, copy the newer file over it and keep the first comment line. The voxel world grew from 64 to 96 cells across to fit the 84 × 84 room.

Only what this map needs came across from the full edition:
- **Materials:** guide amber, cyan beacon, teal shortcut mark, sandstone and slate, on walls and as floor paint. The guide colours glow faintly.
- **Left Shift sprint:** 8.4 m/s while moving and standing, with the sprint pose, a wider view and the backpack glow. Crouching stops it. It works in every level and can be rebound in Settings.
- **Pressure plates and blast doors:** one step on a plate unlocks its door for good, and only a restart seals it again. Doors and plates can stand on upper floors, and a plate only counts a courier standing on its own level.
- **Portals:** wake each end with E. Once both are awake, step into either one to cross to the other. The arrival end waits until you step off it.
- **Cores:** the full edition's core: a bobbing capsule between white caps, with a halo, a light column and rising motes.
- **Floor traps:** the full edition's electric plates replace the spikes in every level. Each tile has a glowing cross: teal while safe, flickering amber in its warning second, red while live. Live plates crackle with arcs between points and up into the air. The arcs are drawn from the clock rather than spawned as particles, so 48 plates never fill the 128-particle pool. Damage needs the feet on the plate.
- **Mission HUD:** Helios Yard shows no words in play: no messages, prompts or key line. A door glyph always marks the exit and is pinned to the screen edge, with an arrow, when the exit is off screen. It is amber while sealed and mint once open. Armour, the seven core slots and the 10:00 countdown stay. The two original yards keep their teaching text.

**Level changes and ground dust.** Every way into a level has a plain title card: a small kicker line, the level name, and a thin cyan rule drawing out under it, over black. The clock holds until the card has gone. After a win, Enter fades out (0.55 s), shows "LEVEL n" for about 1.5 s, then fades in, 2.8 s in all. Start from the title menu begins on black with the card, then fades in, 2.25 s. Restart (R, or Restart in the pause menu) is shorter: a 0.3 s fade, a "RESTART / LEVEL n" card, and back in, 1.5 s. Running kicks a small grey puff from the planted foot on each step (a little larger when sprinting), a jump leaves a puff at take-off, and a landing throws a ring that grows with the fall. The puffs are soft alpha blobs from the same 128-particle pool, and they swell and fade within about half a second. Captures: `--next` (or `--change start|restart`) starts a change at once; `--effect run` and `--effect land` freeze a sprint or a landing.

**Brighter sparks.** Every spark and light streak is drawn as a wider coloured streak with a white-hot core, a longer trail and a bigger, brighter halo. Debris chips are larger and glow hot as they fly. The emitters keep their particle counts but make bigger particles: impact sparks are hotter orange with pale-yellow ones mixed in. A hit also flashes warm light at the chest for a moment. The shock-plate arcs, their landing flashes and the warning embers are brighter too, and the ground dust is a little denser.

Nothing else was synced: no checkpoints, pickups, crimson tiles, slides, wall actions, training, sounds, shadows or bloom. `lite_tests` also covers Helios Yard: the map's pieces and materials, a clear spawn, permanent door unlocks (ground and upper floor), portal linking and crossing, restart, shock damage and sprint. Captures: `CoreExplorerLite --play --level 2 --overview --frames 40 --capture overview.png`; `--warp x y z` starts the courier elsewhere.

## Coursework reference

The original assignment requirements are retained in [docs/ASSIGNMENT.md](docs/ASSIGNMENT.md). ProjectPlan.pdf is the previously submitted plan for the earlier version; it has not been rewritten to describe this reduced build. This build now includes a core-collection objective and exit completion; a final report is still not included, and this is not a claim of full assignment compliance.

## Core collection tutorial

Each map has three blue cores. Approach and press the interact key (E by default); collecting all three opens the exit at the far-right end of the yard. Walk through the door to complete the level, then Enter selects the next map or R restarts. Progress and context hints appear in the plain HUD. Pickup uses a short reach animation and disappears; the exit indicator turns green and the two panels slide apart. No explosive particles, cinematic, or pickup camera shake is added. Reset clears collected cores and closes the gate.

## Member-B integration

Integrated JerryX411's skybox contribution from Member-B commit db8a1acf1a2562a64f99932586163ae0706976a1 using an author-preserving cherry-pick. Includes cubemap rendering, menu panels, surface detail adjustments and the tutorial passage change. The preceding a08ef51 commit only packages build tools/dependencies and generated output; it is intentionally excluded from the source checkout. The merged version uses the Member B cubemap by default (see docs/SKYBOX.md). The procedural sky shaders remain in the source tree but are not used by the renderer.

## Lite timer and impact feedback

The Tutorial Yard has unlimited time (an infinity glyph). The Obstacle Yard starts with a 120-second countdown, displayed as minutes:seconds and highlighted during the last 20 seconds. Timing pauses with the pause/settings menu or loss of focus and freezes on completion/failure. At zero, TIME UP ends the challenge without applying damage. Restart or level change restores the appropriate limit. Result panels show remaining time or UNLIMITED.

Damage briefly recoils the torso/head/arms in the hit direction and emits 24 small sparks/metal fragments. The fixed 128-particle pool uses gravity, drag, voxel, moving-obstacle and fixture collision/bounce and lifetime cleanup, including after failure; pausing freezes the simulation. No continuous emitter is active. The background uses the Member B cubemap. Lighting uses ambient, diffuse and specular terms without additional shadow or effect-light passes.

Core collection emits one short burst of 20 cyan-white sparks; impact sparks are amber with slightly longer luminous streaks. Both reuse the bounded particle pool and expire automatically.

## Jump pads

Each yard has one flush cyan chevron pad (tutorial: 10,1,19; obstacle: 26,1,12). Step or land on it to launch automatically about 4.3 cells above the floor, versus roughly 1.8 for a held normal jump. No Space press/hold is needed; steer in the air with WASD. A cyan-white upward plume and a 0.4-second trail beneath the ascending robot mark launch; particles expire within 0.72 seconds. Landing on it bounces again, while passing above it does not trigger it. Normal ceiling and wall collisions still apply.

## Graphics demonstration (Normal Mapping + Particles)

Normal mapping is enabled by default. During play: **F1** toggles diagnostics, **F2** toggles normal mapping, **F3** shows OFF on the left and ON on the right, **F4** freezes/unfreezes simulation, **F5** cycles normal strength (0 to 2), **F6** toggles hidden-face removal for the core optimisation comparison. These keys are reserved from gameplay rebinding. Diagnostics show camera position, FOV, geometry/draw counts and live particle occupancy. The normal HUD remains frameless and unchanged with diagnostics closed.

See [docs/CHALLENGES.md](docs/CHALLENGES.md) for implementation, reproducible demonstrations and the exact benchmark procedure. No third challenge is claimed: hidden-face removal is the core optimisation. Linux/VLAB verification and the final assessed report remain outstanding.

## Space-test-facility materials

Five original procedural material layers replace the single shared surface texture: warm-white ceramic wall/shell panels, cool dark tread-steel floors, brushed alloy doors/equipment, matte rubber base strips/blocks, and painted casings. Each has its own albedo and tangent-space normal texture plus specular exponent/strength. Cyan remains the interaction/energy accent; amber/red remain hazard feedback. Wall panel joints and cool floor lane bands help orientation. F2/F3/F5 compare the normal detail on all layers. No Portal 2 textures or other game assets are copied. This remains the existing Blinn-Phong material system, not an additional PBR challenge.
