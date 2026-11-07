# Lite graphics demonstrations

The two selected challenges are **Normal Mapping** and **Particles**. Hierarchical models are outside this update. These notes describe code and reproduction, not a claim of marks or a final Report.pdf.

## Interactive checks

Start a level; use F1 to reveal diagnostics (hidden by default). F2 enables/disables normal mapping; F3 splits the same frame at its physical framebuffer midpoint (left OFF, right ON); F4 freezes simulation including timer, hazards and particles; F5 cycles normal strength through 0, 0.5, 1, 1.5, 2; F6 enables/disables hidden-face removal. Mouse orbit/zoom still works during frozen inspection. Pause/settings also stop simulation. Return F4 to OFF to resume gameplay.

Normal mapping adds no vertices. Verify identical triangle counts when switching F2. Inspect the floor, walls and rotated/scaled robot parts. Strength zero uses the same shading path as OFF. The comparison affects mapped surfaces, not the skybox or emissive particles.

## Normal mapping implementation

`RelayRenderer.cpp` generates five periodic 64x64 albedo/normal texture-array layers for ceramic, tread steel, brushed alloy, rubber and painted casing. Each layer has its own height field. Central differences are scaled by texel spacing (32 for a central difference over two texels of a unit UV tile), encoded from [-1,1] to [0,1], uploaded as linear RGB texture-array data and mip-filtered. No downloaded texture is used for this normal map.

The vertex shader transforms normals by inverse-transpose and positions to world space. The fragment shader derives U and V directions from `dFdx/dFdy` of world position and UV, orthogonalises U against the interpolated normal, and obtains bitangent handedness from V. This handles cylinder-cap UV orientation and non-uniform object scaling. A mesh-tangent fallback handles degenerate derivatives. The sampled normal is decoded, its XY strength adjusted, normalised, transformed through TBN, and used by the existing diffuse/specular lighting. The basic lighting model remains Blinn-Phong; no BRDF challenge is claimed.

## Particle implementation

A fixed pool of 128 particles is reused rather than growing indefinitely. Event bursts are bounded; a full pool skips additional emissions. Core pickup emits 20 cyan-white sparks, damage emits 24 amber sparks/fragments, and a jump pad emits 20 upward streaks followed by three trail particles every 0.04 seconds for up to 0.4 seconds of ascent. The pool was enlarged to avoid clipping ordinary overlapping gameplay bursts.

Simulation uses substeps no larger than 1/120 second, exponential drag, gravity, per-axis collision and damped bounce. Collision tests include voxels, moving obstacles and fixtures such as gates/switches. Lifetime reaches zero and the slot becomes reusable. Effects continue to settle after win/loss while the run timer remains frozen; pause or F4 freezes all updates. Reset clears particles and trail state. The normal update path caps a single supplied dt to one second (the game loop caps it to 0.1 seconds).

Fragments use opaque shaded geometry. Sparks use velocity-aligned streaks; launch particles use vertical streaks. Both have additive glow. Geometry is batched/instanced by primitive/blend type; emissive particles depth-test against the scene without depth writes. Width, alpha and streak length fade near expiry. These are lightweight 3D particles, not a screen-space smoke/fluid simulation.

## Reproducible captures

Run from the directory containing the executable and assets. On Linux replace CoreExplorerLite.exe with ./CoreExplorerLite.

```
CoreExplorerLite.exe --play --freeze --frames 2 --normal-off --capture off.png
CoreExplorerLite.exe --play --freeze --frames 2 --capture on.png
CoreExplorerLite.exe --play --freeze --frames 2 --normal-strength 0 --capture zero.png
CoreExplorerLite.exe --play --freeze --frames 2 --compare-normals --diagnostics --capture split.png
CoreExplorerLite.exe --effect core --frames 2 --diagnostics --capture core.png
CoreExplorerLite.exe --effect impact --frames 2 --diagnostics --capture impact.png
CoreExplorerLite.exe --effect pad --frames 2 --diagnostics --capture pad.png
```

`--effect` prepares a deterministic gameplay event and freezes the result. To watch it settle interactively, omit --frames and press F4. These are demonstration entry points, not new game rules. Automated gameplay tests also check overlap, lifetime cleanup, cooldown, restart and repeated jump-pad triggers.

## Core optimisation benchmark

```
CoreExplorerLite.exe --benchmark --level 0 --benchmark-output level0.csv
CoreExplorerLite.exe --benchmark --level 1 --benchmark-output level1.csv
```

Build Release before measuring. The tool requires and verifies a 1920x1080 framebuffer, disables VSync, freezes each level after 24 simulation steps of 1/120 seconds on its jump pad (including its active particles), fixes eye=(20,17,38), target=(20,1,19), vertical FOV=50 degrees, near=0.1, far=300, normal mapping ON at strength 1, skybox ON. Only hidden-face removal differs between baseline and optimised. Both modes keep the same instancing; it is not claimed as an extra challenge.

Each row has 60 unmeasured warm-up scene frames and exactly 240 measured frames. Three trials alternate order. CPU wall time brackets the complete scene draw with glFinish synchronization. Loading, mesh construction, static instance upload, gameplay, UI, screenshots and buffer swapping are excluded. All scene geometry, skybox and frozen particle draws are included. This measures synchronized scene-render time, not gameplay FPS, CPU update cost or a pure GPU timer query.

CSV rows contain mean ms/frame, submitted triangles, draw calls, solid voxels, exposed faces and live particles. The header records GPU/driver and frozen conditions. Two PNGs next to the CSV capture the first baseline and optimised result for image comparison. Resizing/cancelling produces an error; do not use incomplete CSVs. Output parent directories must exist. Run on the intended assessment hardware too; local Windows numbers are not VLAB validation.

## Local measurement record (2026-11-08, before the five-material art pass)

These retained measurements precede the five-material update and must be rerun for final submission. Windows x64 Release on NVIDIA RTX 4060 Laptop GPU, driver 621.01. Raw rows: `benchmarks/windows-20261108-level0.csv` and `benchmarks/windows-20261108-level1.csv`.

| Scene | Baseline mean of 3 trials (ms) | Optimised mean (ms) | Submitted triangles, before / after | Draw calls |
|---|---:|---:|---:|---:|
| Tutorial | 0.367551 | 0.180600 | 68001 / 55037 | 8 / 8 |
| Obstacle | 0.363275 | 0.189072 | 69437 / 56389 | 8 / 8 |

The first baseline trial is slower than later ones: retain all raw values rather than selecting the fastest. These figures describe this frozen scene on this machine, not a general FPS promise. The captured baseline/optimised images are not bit-identical: mean absolute per-channel differences are below 0.009 on a 0..255 scale. Inspect the captures alongside timing results; do not claim exact pixel equivalence. Normal strength zero versus OFF was separately verified pixel-identical. Full-resolution captures and verification.json are in the local Challenge-Checks-20261108 output folder.
