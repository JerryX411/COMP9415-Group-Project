# Courier animation references

Prototype 0.5 studies the readability and personality of Astro Bot through publicly available PlayStation material. The courier's geometry, rig, poses and motion code are independently authored for this project. No Astro Bot model, animation clip, texture or proprietary rig has been imported. This prototype does not claim to reproduce the original game's full movement system.

## Public sources

- [Team Asobi: a unified vision for fun](https://blog.playstation.com/2024/09/09/astro-bot-how-team-asobi-created-a-unified-vision-for-fun/) — the animation team's emphasis on playful emotions, gameplay-first decisions and repeated playtesting.
- [The evolution of Astro Bot's character design](https://blog.playstation.com/2024/08/26/the-evolution-of-astro-bots-adorable-character-design/) — compact proportions, childlike movement, expressive LED eyes and readable mechanical joints.
- [Official behind-the-scenes, episode 4](https://www.youtube.com/watch?v=UyM16UZyb40) — sampled around 00:30-00:40 for head tilt, arm gestures and happy-eye expression.
- [Official behind-the-scenes, episode 1](https://www.youtube.com/watch?v=aH0haX-5iZ8) — sampled gameplay around 00:37 for the body motion accompanying an action. These are selected observations, not a complete frame-by-frame analysis.

## Translation into this courier

| Observed principle | Our implementation |
|---|---|
| Small actions communicate intention and emotion | Looking around, wrist checks, blinking, happy eyes and a brief recovery celebration |
| The whole body participates in movement | Shared pelvis, torso counter-rotation, head lead, banking and antenna follow-through |
| Poses remain easy to read during play | Distinct take-off, tuck, extension and impact poses; directional recoil and a slumped shutdown pose |
| Playfulness supports responsive controls | Pose-only anticipation and settling driven by actual input and velocity; no new input delay |

Foot-plant IK, curve timings, spring constants and collision-envelope limits are our engineering choices. The public sources do not specify Astro Bot's internal implementation, so these should not be presented as its algorithms or exact animation measurements.

## Verification and limits

The same pose builder runs in the game, main-menu preview and F3 workshop. Automated tests cover stance-foot motion, bounded joint changes, the visible geometry envelope, animation events and the existing gameplay regressions. Native OpenGL frames were captured for sprint, jump/landing, recovery and shutdown. Visual checks support further tuning; they are not a claim that the prototype matches Astro Bot's production polish.

Movement still uses a procedural rig rather than authored skeletal clips or motion capture. Feet follow a flat-ground gait; there is no per-foot slope IK or obstacle-aware hand placement. The vertical body collider remains a gameplay approximation, so bounded animation does not imply exact per-limb collision. VLAB/Linux and the user's installed IME still require a manual play session.
