# Core Explorer Lite

## Download and play (Windows x64)

Download **Core-Explorer-Lite-Windows-x64.zip** from [GitHub Releases](https://github.com/JerryX411/COMP9415-Group-Project/releases/tag/lite-2026-11-07). Extract the whole ZIP, then double-click **Play.cmd**. No compiler, CMake, Python or build step is required. Keep the assets folder beside the executable. Requires 64-bit Windows and an OpenGL 3.3-capable graphics driver. GitHub's Code > Download ZIP is source code, not this ready-to-play package.


The linked release is the November 7 snapshot; newer local graphics demonstrations require rebuilding this source.

Current merged Lite version (Member-B): the reduced edition of Core Explorer. The advanced version is preserved in Git history (48f981d) and the separate Core-Explorer repository.

Includes three levels: a compact tutorial showcase, Helios Yard and Corona Run (see below; the old Obstacle Yard was removed on 2026-11-09), plus robot movement/animation, WASD, Left Shift sprint, mouse camera, Space jump, C toggle crouch, E switch interaction, electric floor plates, moving blocks, armour, restart/pause, and a plain UI over a procedural star sky (see [docs/SKYBOX.md](docs/SKYBOX.md)). E disables the linked floor plate for five seconds. R restarts; Esc pauses. The title menu lists, top to bottom and centred: Tutorial, Level 1 (Helios Yard), Level 2 (Corona Run), Settings and Quit. Picking a level starts it; level title cards use the same numbering (TUTORIAL, LEVEL 1, LEVEL 2).

Removed: arrival cinematic, boost/slide/wall actions, jetpack, advanced pickup effects, training, workshop, advanced particle effects, and the full audio pack. Corona Run includes its own checkpoint beacons and fall recovery. The reduced build is intentionally silent.

Build: `cmake -S . -B build`, then `cmake --build build`. Run `build/CoreExplorerLite.exe` (Windows) or `build/CoreExplorerLite` (Linux). Tests: `ctest --test-dir build --output-on-failure`.

The pause menu darkens the game behind it and shows only "Paused" and a centred column of four options, without the subtitle or key hint (`--pause` opens on it for captures). Title, pause, settings and result screens use large bold text without panels, borders or button fills; only the pause menu adds a fullscreen dimming layer. Selected menu text is cyan; the gameplay HUD mirrors the advanced layout: five slanted armour cells at top left, filled/dim hexagonal core slots at bottom centre, and large countdown digits (or an infinity glyph in training) at top right, all without backing panels or outlines. A small glyph shadow preserves readability.

UI uses Segoe UI Bold from Windows when available, or the bundled Rajdhani font under its included OFL licence. Robot and map geometry are generated in code.

## Tutorial showcase

Choose **Tutorial Yard** (level 0) to demonstrate all current Lite mechanics in one 32 by 32 open yard. Its authored map is `assets/levels/tutorial_showcase.txt`. The timer is unlimited; gameplay has no teaching text, interaction prompts or control legend. Armour, core slots, the infinity symbol, exit marker and the teammate's minimap remain. Menus, result screens and optional F1-F6 graphics diagnostics still work.

From the south entrance, the central aisle is clear for walking, strafing, sprinting and jumping. The low block and one-cell crawl tunnel are on the left; the electric floor, its E-operated relay and an independent moving block track are on the right. The north-west alcove demonstrates a permanent ground-plate door. The north-east platform combines a jump pad, ordinary-jump stairs, an upper-floor plate and door. Two nearby portal endpoints can be activated separately and then traversed in either direction. Three cores (centre, ground alcove, upper platform) unlock the north exit. Every station is accessible without completing a long puzzle chain; R resets the entire display.

Ceramic, tread, rubber, alloy and painted surfaces are visible throughout the yard, including the small material row near the entrance. Running, jumping, landing, launch, damage, pickup and portal feedback use the existing particle effects. `tutorial_showcase` in CTest walks a complete collection/door/portal/jump-pad/upper-door/exit route without warping, then checks restart and the normal-jump staircase. `lite_gameplay` also checks the crawl passage, switch, hazards and feedback in this layout.

## Lives

Each timed level gives **three lives**. The tutorial has unlimited retries. Losing all five armour cells, to hits or to falls into the void, costs a life. The robot then topples, and a 24-segment reboot bar fills, with "REBOOTING // n LIVES LEFT" over it in levels that show text. After 2.2 s the screen fades to black and the robot beams back in at the last checkpoint (or the start) with full armour and a second of grace. Losing the third life shows a red "ALL LIVES LOST" banner (text levels only), fades out, and restarts the level automatically through the restart title card. Time and collected cores carry over through a reboot. Running out of time still ends the attempt immediately.

The HUD shows lives as three little robot heads under the armour cells, in glowing cyan while alive and faded once spent; the one just lost flashes red. The tutorial yard never runs out of lives: one head and an infinity sign. Captures: `--effect reboot` freezes a reboot in progress.

## Corona Run (level 3, the second mission, in development)

**Corona Run** comes after Helios Yard: choose **Level 2** on the title screen (or `--level 2`), or press Enter after winning Helios. Helios is an open yard to explore and puzzle through. Corona Run is a long timed parkour path built for challenge (rebuilt and extended on 2026-11-09). Nine main strands wind round a 76 × 76 room twice, climbing from stand height 4 to 12 and back down. Four short side branches with grey-teal floors hold four of the cores. The time limit is 5:00, and ten cores open the exit, so every branch has to be visited. Corona Run is the last level: after the win, the result screen offers only **ENTER MAIN MENU**, which fades back to the title screen (R does nothing there).

- **The void:** under the path is a death floor (floor paint `9`) drawn like the full edition's crimson tiles. It is a near-black red bed of cells, each flickering dark red like a failing light on its own beat, with a slower glow shared across 6 × 6 blocks and a soft wave. The shader draws the cells from the clock. Crimson embers and dark ash drift up from the void near the robot. Landing on it costs one armour cell and returns the robot to the last checkpoint. Losing all five cells costs one of the three lives (see Lives).
- **Checkpoints:** only four, so a mistake costs a long stretch. They stand at the corners after S2, N2, S4 and S6. Reaching one saves the respawn point; it turns from amber to mint and raises a column of light.
- **Parkour types:**
  - gaps at walking and sprint distance, including flat six-cell sprint gaps;
  - a six-cell long jump that lands two blocks lower;
  - hurdles to jump, and low bars that are too high to jump, so you crouch under them;
  - 1 × 1 stepping stones in rising and dropping zigzags;
  - 1 × 1 stones shocked in turn, so you hop on the beat;
  - a pillar climb;
  - a one-cell sky beam, and a zigzag beam that turns two corners at every jog;
  - a chain of two jump pads (6 → 9 → 12);
  - a jump pad onto a wall top that holds a core;
  - blocks sliding along the path, to jump over (both axes);
  - sweepers crossing the path;
  - shock strips, including a rolling wave;
  - a crawl tunnel with a shock floor.
- **Main path:**
  - **S1** (east, north edge): gaps, a hurdle, a low bar, rising zigzag stones, a shock strip, a five-cell sprint gap and a sweeper.
  - **S2** (south, east side): junction P.
  - **N1** (west, through the north): the two-pad chain, the zigzag beam, the long jump down, then hurdles and a low bar.
  - **N2** (south, west side): the shocked stones.
  - **S3** (east, through the middle): a pillar climb, a shocked sky beam crossed by two long sweepers, fork F, a sliding block, a sprint gap and the shocked crawl tunnel.
  - **S4** (south, east side): a descent over dropping zigzag stones.
  - **S5** (west, along the south): a rolling shock wave, a sweeper, junction J, then the main stretch or the B5 loop to junction K, then hurdles and a low bar.
  - **S6** (south, west side): a sliding block, a sprint gap and a shock strip.
  - **S7** (east, south edge): the jump pad onto the wall top, a sweeper, a flat six-cell sprint gap, a sliding block and a shock strip. Then cross the court's west wall and drop into the exit court.
- **Side branches** (one or two cores each):
  - **B1:** a jump pad on the inner edge of S1 lifts you to a perch. The centre line misses the pad, so step on it deliberately. Drop back down.
  - **B2:** 1 × 1 stones climb east from junction P to a tall perch. Hop back the same way.
  - **B3:** from fork F, a beam with a shock strip runs back west beside S3 to two platforms. Return the same way. There is no portal.
  - **B5:** a loop south of S5 over zigzag stones, rejoining at K.
- **Hazards that cover two routes:**
  - **Junction P:** one shock field covers the whole junction. You cross it going down S2, into B2, and back out.
  - **Sweepers XA and XB:** both cross the S3 sky beam and the B3 beam beside it. You meet each one three times: east on S3, then west and east on B3. While one beam is swept, the other is clear.
  - **Junction K:** one shock field covers the junction where the B5 loop and the S5 main stretch meet.
- **Edges:** amber trim marks every jump lip and stepping stone. The main strands are at least seven cells apart across the void, or out of reach above, so no stretch of the main path can be skipped by jumping across. You can still hop into a branch early.
- **Engine changes:** traps and movers take an optional trailing level in the level file, like doors and plates, so they can stand on raised floors. The spawn now stands on whatever is built at its spot.

The map is generated by `tools/generate_corona_run.py`, which also writes `tests/corona_route.txt`. The generator lays the path like a turtle, one piece at a time, and records the bot's route as it goes. The pieces are: run, gap, hurdle, low bar, shock strip, sweeper, sliding block, tunnel, pillars, stones, shocked stones, zigzag beam and corner.

In CTest, `corona_run` checks the pieces: 10 cores, 4 checkpoints, 4 jump pads and no portals. It also checks falling and respawning, checkpoints, losing after five falls, lives and restarting.

A bot then finishes the whole run, every branch included, using only key presses. It runs, sprints and jumps at lips by itself, crouches under bars, waits for shock strips and sweepers, jumps sliding blocks on either axis, and rides pad chains. It finishes in about 141 s with no falls and no damage, so the 5:00 limit leaves little slack for a first attempt.

## Helios Yard (four-region Lite layout)

Choose **Level 1** on the title screen (or `--level 1`). Enter after a win moves through Tutorial Yard → Helios Yard → Corona Run. Its authored map is `assets/levels/helios_yard.txt`, in `LevelData` format. This Lite layout is maintained separately from the full edition: do not replace it with the full edition's map.

The yard now contains **four regions and four cores**: the northwest roof, northeast shock floor, southwest layered maze and southeast corridor maze. The middle west maze, middle east stepping-stone trial and central core tower were removed. Removing 24 rows reduces the room from 84 × 84 to **84 × 60**, with a level cross-passage, a new four-cell-wide staircase to the northwest roof, and entry plates on the accessible side of their doors. Six blast doors and their six plates remain, including the central exit approach and elevated east shortcut. The portal pair, jump pad, hazards and final exit remain. All four cores unlock the final exit; the countdown remains 10:00.

`helios_four_core_route` plays from spawn through all four cores, the linked pressure plates and the final exit using movement, jump, crouch and interact inputs. It never warps the player or injects inventory/door state. The checked route finishes in about 101 simulated seconds without damage or falls; this is an automated route, not a first-time player's completion estimate.

**Random shock floor:** the big electric floor in the east room (the 31 plates between x 60–84 and z 14–30, around the core at 71.5, 26.5) no longer fires all at once on a fixed beat. Each of these plates has its own offset. In every 6-second slot its 1-second warning starts at a random moment between 0.5 and 2.5 seconds, followed by 2 seconds of shock. So the floor crackles in a shifting patchwork and you have to read the amber warnings to pick a way across. The warning always comes first, and at least 1 second of safe floor separates two shocks on the same plate. The schedule is a hash of the slot and the plate, so it is the same on every attempt. The other plates in Helios keep the fixed beat. In the level file, these plates end with `1 random` (the trailing level, then the word `random`).

Only what this map needs came across from the full edition:
- **Materials:** guide amber, cyan beacon, teal shortcut mark, sandstone and slate, on walls and as floor paint. The guide colours glow faintly.
- **Left Shift sprint:** 8.4 m/s while moving and standing, with the sprint pose, a wider view and the backpack glow. Crouching stops it. It works in every level and can be rebound in Settings.
- **Pressure plates and blast doors:** one step on a plate unlocks its door for good, and only a restart seals it again. Doors and plates can stand on upper floors, and a plate only counts a courier standing on its own level.
- **Portals:** wake each end with E. Once both are awake, step into either one to cross to the other. The arrival end waits until you step off it.
- **Cores:** the full edition's core: a bobbing capsule between white caps, with a halo, a light column and rising motes.
- **Floor traps:** the full edition's electric plates replace the spikes in every level. Each tile has a glowing cross: teal while safe, flickering amber in its warning second, red while live. Live plates crackle with arcs between points and up into the air. The arcs are drawn from the clock rather than spawned as particles, so 48 plates never fill the 128-particle pool. Damage needs the feet on the plate.
- **Mission HUD:** Helios Yard shows no teaching messages or interaction prompts. A door glyph marks the exit and is pinned to the screen edge, with an arrow, when off screen. It is amber while sealed and mint once open. Armour, four core slots and the 10:00 countdown remain. The tutorial showcase and Corona Run use the same symbol HUD; fixed-camera mode adds a small V toggle indicator.

**Level changes and ground dust.** Every way into a level has a plain title card: a small kicker line, the level name, and a thin cyan rule drawing out under it, over black. The clock holds until the card has gone. After a win, Enter fades out (0.55 s), shows "LEVEL n" for about 1.5 s, then fades in, 2.8 s in all. Start from the title menu begins on black with the card, then fades in, 2.25 s. Restart (R, or Restart in the pause menu) is shorter: a 0.3 s fade, a "RESTART / LEVEL n" card, and back in, 1.5 s. Running kicks a small grey puff from the planted foot on each step (a little larger when sprinting), a jump leaves a puff at take-off, and a landing throws a ring that grows with the fall. The puffs are soft alpha blobs from the same 128-particle pool, and they swell and fade within about half a second. Captures: `--next` (or `--change start|restart`) starts a change at once; `--effect run` and `--effect land` freeze a sprint or a landing.

**Brighter sparks.** Every spark and light streak is drawn as a wider coloured streak with a white-hot core, a longer trail and a bigger, brighter halo. Debris chips are larger and glow hot as they fly. The emitters keep their particle counts but make bigger particles: impact sparks are hotter orange with pale-yellow ones mixed in. A hit also flashes warm light at the chest for a moment. The shock-plate arcs, their landing flashes and the warning embers are brighter too, and the ground dust is a little denser.

Nothing else was synced: no checkpoints, pickups, crimson tiles, slides, wall actions, training, sounds, shadows or bloom. `lite_tests` also covers Helios Yard: the map's pieces and materials, a clear spawn, permanent door unlocks (ground and upper floor), portal linking and crossing, restart, shock damage and sprint. Captures: `CoreExplorerLite --play --level 1 --overview --frames 40 --capture overview.png`; `--warp x y z` starts the courier elsewhere.

## Coursework reference

The original assignment requirements are retained in [docs/ASSIGNMENT.md](docs/ASSIGNMENT.md). ProjectPlan.pdf is the previously submitted plan for the earlier version; it has not been rewritten to describe this reduced build. This build now includes a core-collection objective and exit completion; a final report is still not included, and this is not a claim of full assignment compliance.

## Core collection tutorial

The tutorial has three blue cores, Helios seven and Corona Run ten. Approach and press the interact key (E by default); collecting every core opens that yard's exit. Walk through the door to complete the level, then Enter selects the next map or R restarts. After Corona Run, the last level, the only option is Enter for the main menu. Filled HUD slots show collection progress. Pickup uses a short reach animation and a spark burst; the exit indicator turns green and the two panels slide apart. Reset clears collected cores and closes the gate.

## Member-A / Member-B integration (2026-11-10)

Member-A `1af555e` is merged into Member-B `caab965`, retaining both branch histories. The combined game keeps the tutorial showcase, Helios Yard (level 1), Corona Run (level 2), checkpoints, lives/reboot feedback, procedural sky and transparent menus from Member-A, plus Member-B's camera modes and rotating minimap.

The minimap is visible in all three levels by default. Press **M** to show or hide it for the current session; M is reserved from key rebinding. The circular map follows 70% of a camera turn, then smoothly settles to the view heading. It shows the current floor, remaining cores, doors, controls, portals and exit. The exit HUD marker avoids the map only while it is visible. While moving, the robot follows the camera heading (including strafing and reversing); when stopped, the camera can orbit without turning the robot. Tunnel probes use actual movement direction and do not force the idle orbit behind the robot.

The earlier minimap contribution `c48d28c` and camera/map update `caab965` retain their original author. The historical cubemap contribution remains available in Git history and the legacy Skybox source/assets; the current renderer uses the procedural sky described in [docs/SKYBOX.md](docs/SKYBOX.md).

Member-B's bundled `vendor/` dependencies, `toolchain/` and legacy `Playable/` package are retained. Build this merged source with **BuildAndRun.cmd** (or `BuildAndRun.cmd --build-only`); **Play.cmd** delegates to it so the legacy executable is not mistaken for the merged build. Incremental builds only rebuild changed files. `SOURCE-README.md` and `Playable/` describe an older snapshot; use this README and `build/CoreExplorerLite.exe` for the current version. The November 7 GitHub Release is also unchanged by this source merge.

Validation includes the existing `lite_gameplay`, `tutorial_showcase` and `corona_run` CTest suites, including the teammate's idle-orbit/moving-follow tests and both new level routes.

## Lite timer and impact feedback

The Tutorial Yard has unlimited time (an infinity glyph). The missions count down from their own limits (Helios 10:00, Corona Run 5:00), displayed as minutes:seconds and highlighted during the last 30 seconds. Timing pauses with the pause/settings menu or loss of focus and freezes on completion/failure. At zero, TIME UP ends the challenge without applying damage. Restart or level change restores the appropriate limit. Result panels show remaining time or UNLIMITED.

Damage briefly recoils the torso/head/arms in the hit direction and emits 24 small sparks/metal fragments. The fixed 128-particle pool uses gravity, drag, voxel, moving-obstacle and fixture collision/bounce and lifetime cleanup, including after failure; pausing freezes the simulation. No continuous emitter is active. The background is a procedural star sky. Lighting uses ambient, diffuse and specular terms without additional shadow or effect-light passes.

Core collection emits one short burst of 20 cyan-white sparks; impact sparks are amber with slightly longer luminous streaks. Both reuse the bounded particle pool and expire automatically.

## Camera view

Entering or restarting a level always starts in the **third-person follow camera**. Press **V** during play to switch to the fixed **whole-map view**, and press **V** again to return. The high, north-up orthographic camera fits the entire level, including elevated geometry and jump headroom. It automatically fits the window aspect ratio and leaves space for the HUD. A cyan locator marks the courier even under upper floors; the minimap is hidden in this view. Mouse movement, the wheel, footsteps, jumps, damage, portals and respawns do not move the shot. WASD matches the displayed map directions. Press V again to restore the follow camera with the previous orbit and zoom. Loading a different map resets to third person; switching to the global camera then fits the new map. V is reserved from rebinding; `--surveillance` opens directly in the whole-map view for captures.

## Jump pads

Each yard has a flush cyan pad with two concentric rings and a centre dot (tutorial showcase: 29.5,1,19; Helios: 81.5,1,14.5; Corona Run has four: the B1 perch pad at 43.5,6,73.5, the N1 chain at 61.5,6,57.5 and 57.5,9,57.5, and the S7 wall pad at 16.5,7,6.5). The circular marking is drawn procedurally with antialiased edges and brightens on launch. Step or land on it to launch automatically about 4.3 cells above the floor, versus roughly 1.8 for a held normal jump. No Space press/hold is needed; steer in the air with WASD. A cyan-white upward plume and a 0.4-second trail beneath the ascending robot mark launch; particles expire within 0.72 seconds. Landing on it bounces again, while passing above it does not trigger it. Normal ceiling and wall collisions still apply.

## Graphics demonstration (Normal Mapping + Particles)

Normal mapping is enabled by default. During play: **F1** toggles diagnostics, **F2** toggles normal mapping, **F3** shows OFF on the left and ON on the right, **F4** freezes/unfreezes simulation, **F5** cycles normal strength (0 to 2), **F6** toggles hidden-face removal for the core optimisation comparison. These keys are reserved from gameplay rebinding. Diagnostics show camera position, FOV, geometry/draw counts and live particle occupancy. The normal HUD remains frameless and unchanged with diagnostics closed.

See [docs/CHALLENGES.md](docs/CHALLENGES.md) for implementation, reproducible demonstrations and the exact benchmark procedure. No third challenge is claimed: hidden-face removal is the core optimisation. Linux/VLAB verification and the final assessed report remain outstanding.

## Space-test-facility materials

Five original procedural material layers replace the single shared surface texture: warm-white ceramic wall/shell panels, cool dark tread-steel floors, brushed alloy doors/equipment, matte rubber base strips/blocks, and painted casings. Each has its own albedo and tangent-space normal texture plus specular exponent/strength. Cyan remains the interaction/energy accent; amber/red remain hazard feedback. Wall panel joints and cool floor lane bands help orientation. F2/F3/F5 compare the normal detail on all layers. No Portal 2 textures or other game assets are copied. This remains the existing Blinn-Phong material system, not an additional PBR challenge.
