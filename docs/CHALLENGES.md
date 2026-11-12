#  graphics demonstrations

The two selected challenges are **Normal Mapping** and **Particles**. The robot and animated exit gate supply the separate core hierarchical-model requirement. These notes describe code and reproduction; `Report.pdf` contains the current evaluation.

## Interactive checks

Start a level; use F1 to reveal diagnostics (hidden by default). F2 enables/disables normal mapping; F3 splits the same frame at its physical framebuffer midpoint (left OFF, right ON); F4 freezes simulation including timer, hazards and particles; F5 cycles normal strength through 0, 0.5, 1, 1.5, 2; F6 enables/disables hidden-face removal. Mouse orbit/zoom still works during frozen inspection. Pause/settings also stop simulation. Return F4 to OFF to resume gameplay.

Normal mapping adds no vertices. Verify identical triangle counts when switching F2. Inspect the floor, walls and rotated/scaled robot parts. Strength zero uses the same shading path as OFF. The comparison affects mapped surfaces, not the background or emissive particles.

## Normal mapping implementation

`RelayRenderer.cpp` generates five periodic 64x64 albedo/normal texture-array layers for ceramic, tread steel, brushed alloy, rubber and painted casing. Each layer has its own height field. Central differences are scaled by texel spacing (32 for a central difference over two texels of a unit UV tile), encoded from [-1,1] to [0,1], uploaded as linear RGB texture-array data and mip-filtered. No downloaded texture is used for this normal map.

The vertex shader transforms normals by inverse-transpose and positions to world space. The fragment shader derives U and V directions from `dFdx/dFdy` of world position and UV, orthogonalises U against the interpolated normal, and obtains bitangent handedness from V. This handles cylinder-cap UV orientation and non-uniform object scaling. A mesh-tangent fallback handles degenerate derivatives. The sampled normal is decoded, its XY strength adjusted, normalised, transformed through TBN, and used by the existing diffuse/specular lighting. The basic lighting model remains Blinn-Phong; no BRDF challenge is claimed.

## Particle implementation

A fixed pool of 128 particles is reused rather than growing indefinitely. Event bursts are bounded; a full pool skips additional emissions. Core pickup emits 20 cyan-white sparks, damage emits 24 amber sparks/fragments, and a jump pad emits 20 upward streaks followed by three trail particles every 0.04 seconds for up to 0.4 seconds of ascent. The pool was enlarged to avoid clipping ordinary overlapping gameplay bursts.

Simulation uses substeps no larger than 1/120 second, exponential drag, gravity, per-axis collision and damped bounce. Collision tests include voxels, moving obstacles and fixtures such as gates/switches. Lifetime reaches zero and the slot becomes reusable. Effects continue to settle after win/loss while the run timer remains frozen; pause or F4 freezes all updates. Reset clears particles and trail state. The normal update path caps a single supplied dt to one second (the game loop caps it to 0.1 seconds).

Fragments use opaque shaded geometry. Sparks use velocity-aligned streaks; launch particles use vertical streaks. Both have additive glow. Geometry is batched/instanced by primitive/blend type; emissive particles depth-test against the scene without depth writes. Width, alpha and streak length fade near expiry. These are lightweight 3D particles, not a screen-space smoke/fluid simulation.

## Reproducible captures

Run from the directory containing the executable and assets. On Linux replace CoreExplorer.exe with ./CoreExplorer.

```
CoreExplorer.exe --play --freeze --frames 2 --normal-off --capture off.png
CoreExplorer.exe --play --freeze --frames 2 --capture on.png
CoreExplorer.exe --play --freeze --frames 2 --normal-strength 0 --capture zero.png
CoreExplorer.exe --play --freeze --frames 2 --compare-normals --diagnostics --capture split.png
CoreExplorer.exe --effect core --frames 2 --diagnostics --capture core.png
CoreExplorer.exe --effect impact --frames 2 --diagnostics --capture impact.png
CoreExplorer.exe --effect pad --frames 2 --diagnostics --capture pad.png
```

`--effect` prepares a deterministic gameplay event and freezes the result. To watch it settle interactively, omit --frames and press F4. These are demonstration entry points, not new game rules. Automated gameplay tests also check overlap, lifetime cleanup, cooldown, restart and repeated jump-pad triggers.

## Core optimisation benchmark

```
CoreExplorer.exe --benchmark --level 0 --benchmark-output level0.csv
CoreExplorer.exe --benchmark --level 1 --benchmark-output level1.csv
CoreExplorer.exe --benchmark --level 2 --benchmark-output level2.csv
```

Build Release before measuring. The tool requires and verifies a 1920x1080 framebuffer, disables VSync, freezes each level after 24 simulation steps of 1/120 seconds on its jump pad (including its active particles), fixes eye=(20,17,38), target=(20,1,19), vertical FOV=50 degrees, near=0.1, far=300, normal mapping ON at strength 1, procedural star background at a fixed render time of zero. Only hidden-face removal differs between baseline and optimised. Both modes keep the same instancing; it is not claimed as an extra challenge.

Each row has 60 unmeasured warm-up scene frames and exactly 240 measured frames. Three trials alternate order. CPU wall time brackets the complete scene draw with glFinish synchronization. Loading, mesh construction, static instance upload, gameplay, UI, screenshots and buffer swapping are excluded. All scene geometry and frozen particle draws are included. This measures synchronized scene-render time, not gameplay FPS, CPU update cost or a pure GPU timer query.

CSV rows contain mean ms/frame, submitted triangles, draw calls, solid voxels, exposed faces and live particles. The header records GPU/driver and frozen conditions. Two PNGs next to the CSV capture the first baseline and optimised result for image comparison. Resizing/cancelling produces an error; do not use incomplete CSVs. Output parent directories must exist. Run on the intended assessment hardware too; local Windows numbers are not VLAB validation.

## Current local measurement record (2026-11-13)

Windows x64 Release, GCC 16.2, NVIDIA RTX 4060 Laptop GPU, driver 621.01. Current raw data, paired images, source fingerprints and test output are retained in `report/evidence/`. Each mean below averages three 240-frame runs, with all raw rows retained.

| Scene | Baseline ms | Optimised ms | Reduction | Triangles, before / after |
|---|---:|---:|---:|---:|
| Tutorial | 2.806 | 2.605 | 7.2% | 93745 / 78109 |
| Helios | 3.426 | 2.801 | 18.3% | 378425 / 221033 |
| Corona | 3.355 | 2.664 | 20.6% | 322725 / 171881 |

These are synchronized fixed-scene render times, not gameplay FPS or cross-device guarantees. Paired screenshots are nearly but not exactly identical; numerical image differences are in `summary.json`. Older `benchmarks/windows-20261108-*` files describe historical scenes and are not current results.
