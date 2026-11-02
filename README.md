# Core Explorer Lite

Current Member-A version: the reduced edition of Core Explorer. The advanced version is preserved in Git history (48f981d) and the separate Core-Explorer repository.

Includes two simple voxel yards, robot movement/animation, WASD, mouse camera, Space jump, C toggle crouch, E switch interaction, retractable spikes, moving blocks, armour, restart/pause, and a plain, static UI. E retracts the linked spikes for five seconds. R restarts; Esc pauses. Select LEVEL in the title menu to change maps.

Removed: arrival cinematic, sprint/boost/slide/wall actions, jetpack, portals, checkpoints, cores, pickups, timers, training, workshop, particle effects, and the full audio pack. The reduced build is intentionally silent.

Build: `cmake -S . -B build`, then `cmake --build build`. Run `build/CoreExplorerLite.exe` (Windows) or `build/CoreExplorerLite` (Linux). Tests: `ctest --test-dir build --output-on-failure`.

Title and settings use a static grey background and ordinary buttons, without the robot showroom, animation, glow or blur. HUD panels are flat.

UI uses Segoe UI from Windows when available, or the bundled Rajdhani font under its included OFL licence. Robot and map geometry are generated in code.

## Coursework reference

The original assignment requirements are retained in [docs/ASSIGNMENT.md](docs/ASSIGNMENT.md). ProjectPlan.pdf is the previously submitted plan for the earlier version; it has not been rewritten to describe this reduced build. This playground currently has no objective/completion condition or final report and is not a claim of full assignment compliance.
