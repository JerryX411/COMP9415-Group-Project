# Simplified coursework background

The lite version draws a procedural star sky (`assets/shaders/sky.vert` / `sky.frag`, since 2026-11-08) behind the game and on the title screen. There is no image: one fullscreen triangle per frame works out each pixel's view direction from the inverse view-projection matrix. It draws:
- a deep navy gradient that is a little bluer toward the horizon;
- a tilted band of milky light with violet and teal haze, plus a few faint nebula patches elsewhere;
- three layers of stars (many small, some medium, a few bright) in white, cool blue and warm tints, twinkling slowly. They are denser inside the band.

The title screen looks out at the same sky and turns very slowly. The overview capture maps the screen onto a patch of sky. There are still no skybox textures, sun discs or nebula images. The earlier teammate skybox integration (a cube-map photo) remains in Git history; its runtime code and assets stay removed.

Lighting retains world-space ambient, diffuse and Blinn-Phong specular shading, with five textured material appearances and tangent-space normal mapping. There is no shadow-map pass, PCF filtering, decorative wall lamps or local effect-light list. Emissive item colours and gameplay particles remain as readable feedback.

Normal Mapping and Particles remain the selected challenges. Their controls and lifecycle are unchanged. Previous benchmark measurements describe older visuals; rerun the benchmark on the final submission version.
