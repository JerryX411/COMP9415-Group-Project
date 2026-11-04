# Skybox asset

`cross.jpg` is an unchanged copy of the user's supplied `web.jpg`.
The source website and reuse licence have not been provided; record them before submitting or redistributing this asset.

Layout: a 4 by 3 horizontal cross, with six equal square faces:

```text
       +Y
 -X    +Z    +X    -Z
       -Y
```

The loader uploads the six tiles directly to an OpenGL cubemap. Blank cells are ignored.
No separate cropped images or new image-processing dependencies are required.
Replace this file with another image using the same layout, then rebuild to copy assets.
The supplied image has limited resolution; the implementation preserves the original pixels.
