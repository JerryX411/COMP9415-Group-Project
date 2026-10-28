# Craft 3D OpenGL Starter for Group Assignment - COMP3421/COMP9415 26T3

## Overview

The codebase is the starter code for the COMP3421/COMP9415 26T3 Computer Graphics group assignment, which requires students to implement a simple Minecraft-style voxel game. You can find the assignment specification in [Group Assignment Specification repo](https://gitlab.cse.unsw.edu.au/coursework/COMP3421/26T3/group-assignment-spec).

The starter code is designed to be a minimal, working 3D OpenGL application that students can build upon. It opens an OpenGL 3.3 window, shows a start screen, and then renders one fixed, coloured tetrahedron, the tetrahedron is deliberately unrelated to the assessed voxel scene. The starter does **not** provide:

- Voxel or chunk storage, block/cube mesh generation, indexed meshing, or a controllable camera.
- Texture-atlas UVs, materials, lighting, humanoid modelling, skin mapping, or animation (Optional).
- Terrain generation or rendering optimisation.

Implement the required features described in the [assignment specification](https://gitlab.cse.unsw.edu.au/coursework/COMP3421/26T3/group-assignment-spec).

**IMPORTANT**: You should at least make sure your codabase could be compiled and ran successfully on the CSE machines (VLAB). When we marking your submission, we will test your submission on the CSE machines.

## What is provided

- A C++ CMake project using OpenGL 3.3 core, GLFW, GLAD 2 (on Windows/Linux) and GLM.
- A  minimal 3D example mesh (tetrahedron) with a fixed transform and colour.
- A start screen titled "Simple Voxel Game" with a pulsing "PRESS THE ENTER KEY" prompt (see [Start screen](#start-screen)).
- Reusable 2D helpers: `Texture` (loads a PNG with stb_image into an OpenGL texture) and `TextRenderer` (draws bitmap-font text and solid rectangles in screen pixels using `assets/textures/font.png`). You can reuse them for your own HUD, such as a  hotbar.
- One compact Dear ImGui window composed from a course-provided benchmark and custom widgets for the example in `src/app/CustomUi.cpp`, you may add your own assignment-specific controls to `CustomUi` to make it easier to develop and debug.

### Optional (For students who have completed COMP6771 Advanced C++ Programming):

- CTest and Catch2 configuration for optional software testing (It will not be assessed, but students who have completed COMP6771 may find them useful as well).

Students are expected to replace/remove `Example3DMesh` and the fixed transform with their own assignment design.

## Project layout

```text
assets/textures/    Textures you need to load and use for your assignment, you need to add your charactor skin texture to this folder. NameMC is a good website to find skins you like.
assets/shaders/     Minimal GLSL shaders, plus the text and start screen shaders
src/app/            AppUi, CustomUi, TitleScreen, and the independent benchmark timer
src/core/           Asset lookup
src/graphics/       Window, shader, texture, text renderer, and tetrahedron example
tests/              Optional Catch2 self-tests
```

Add implementation files to `src/CMakeLists.txt`. Use
`craft::core::assetPath(...)` in `main.cpp` to add GLSL shaders. 

## Build with CMake and test with CTest directly

[CMake](https://cmake.org/cmake/help/latest/guide/tutorial/index.html) is the recommended tool for building the project. Other compatible CMake generators and development environments, like Ninja, are also acceptable.

```bash
cmake -S . -B build # If you have experienced with Ninja, you can add `-G Ninja` to the command line to use Ninja generator

cmake --build build
ctest --test-dir build --output-on-failure  # Optional self-tests
```

## Run the application executable directly

After a successful build, the executable is `build/Craft` on macOS/Linux and
`build/Craft.exe` on Windows:

```bash
./build/Craft
```

## Start screen

`TitleScreen` (`src/app/TitleScreen.cpp`) is shown when the application starts, before the tetrahedron scene and the Dear ImGui window:

- A pixel-art background image, `assets/textures/title_background.png`, with "Simple Voxel Game" fading in over it. The text is drawn with `assets/textures/font.png` through `TextRenderer`.
- "PRESS THE ENTER KEY" appears below the title and pulses slowly.
- Pressing **Enter** (or keypad Enter) hides the prompt immediately, holds the screen for 0.3 seconds, then fades to black over 0.6 seconds before the game starts. **ESC** still closes the window at any time.

The current image (272×153, 16:9) is built from the [Mountain at Dusk Background](https://opengameart.org/content/mountain-at-dusk-background) parallax layers.  Text, timings and layout are constants at the top of `TitleScreen.cpp`.  And, `main.cpp` stops drawing the start screen and releases its GPU resources once `TitleScreen::finished()` returns true.

### Changing the Start Screen

To use a different cover image, replace `assets/textures/title_background.png` with another PNG of the same name and rebuild (the build copies `assets/` next to the executable). No shader or code changes are needed:

- Any aspect ratio works, but 16:9 or 4:3 fit best. The image fills the window without stretching; whatever does not fit is cropped evenly from the centre.
- Small pixel-art images are scaled up with nearest-neighbour filtering, so their pixels stay sharp. Large images are scaled down smoothly.
- If the new image is bright where the text sits, adjust `kTitleCentreY` and `kPromptCentreY` in `TitleScreen.cpp` to move the title and prompt.

If you want to change the title and prompt text, edit `kTitle` and `kPrompt` in `TitleScreen.cpp`. To resize the text, change `kDefaultTitleSize` and `kDefaultPromptSize` (the height of one font cell as a fraction of the window height; defaults 0.15 and 0.042), or call `TitleScreen::setTextSizes(titleSize, promptSize)` at runtime. Either line is still shrunk automatically if it would be wider than `kTitleMaxWidth` / `kPromptMaxWidth` of the window.

## 240-frame benchmark

`AppUi` owns the shared Dear ImGui lifecycle and the course-provided benchmark
section. `CustomUi` remains the separate extension point where students may add
assignment-specific controls; the starter version only rotates or recolours the
disposable example object and exposes no camera controls or camera state. 

Keep the code that renders your scene inside the benchmark region, between
`SceneBenchmark::beginSceneFrame()` and `SceneBenchmark::endSceneFrame()`.
This includes code such as clearing the buffers, setting shader uniforms,
and drawing your voxel mesh. Keep ImGui rendering and buffer presentation
outside this measured region.

To clarify more directly, if you keep current code structure and working in `src/main.cpp`:

```cpp
int main() {
  try {
    // ... omitted for brevity
    while (!window.shouldClose()) {
      // ... omitted for brevity
      benchmark.beginSceneFrame();
      // Your voxel scene rendering code goes here
      const bool benchmarkCompleted = benchmark.endSceneFrame();
      if (benchmarkCompleted) {
        printBenchmarkResult(benchmark.snapshot(), framebufferWidth,
          framebufferHeight);
        }
        // ... omitted for brevity
      }
    } catch (const std::exception& error) {
        std::cerr << "Craft failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
```

Use the **Run benchmark** button, or start an automatic run from a terminal:

```bash
CRAFT_BENCHMARK=1 ./build/Craft     
```

The start screen is **skipped** for the environment-triggered run (`CRAFT_BENCHMARK=1`), so the benchmark starts immediately and never waits for Enter. The start screen is also not part of the measured region: it is drawn before the scene, and the **Run benchmark** button only becomes available after you have pressed Enter.

The benchmark disables VSync, performs 60 warm-up frames, measures exactly 240 scene frames with GPU completion synchronisation, prints `average_ms`, then restores
VSync, and excludes Dear ImGui and buffer swap. An environment-triggered run closes automatically after printing its result.

The starter result is labelled `workload=starter-3d-tetrahedron`. It validates only the benchmark harness and is not assignment performance evidence. **Replace the tetrahedron with your voxel scene between the `beginSceneFrame` and `endSceneFrame` calls** before test your own implementation.

When improving performance, consider how mesh generation and draw-call count
affect the result. Use the benchmark to compare approaches; no specific
optimisation technique is required.

Please check the [assignment specification](https://gitlab.cse.unsw.edu.au/coursework/COMP3421/26T3/individual-assignment-spec) for more details about submission requirements.

## References

There are some recommended references for C++ and OpenGL learning you may find useful for your assignment.

### C++ Basics

- [Learn C++ Programming for Beginners – Free 31-Hour Course](https://www.freecodecamp.org/news/learn-c-with-free-31-hour-course/)
- [Learn C++ from scratch: The complete guide for beginners](https://www.educative.io/blog/how-to-learn-cpp-the-guide-for-beginners)
- [C++ Tutorial - Cplusplus.com](https://cplusplus.com/doc/tutorial/)
- [C++ Tutorial - W3Schools](https://www.w3schools.com/cpp/)

### C++ Advanced

- **[C++ Reference](https://cppreference.com/)** (The most important and up-to-date documents for C++ programming, you may have learned how to use it in COMP6771)
- [Learn C++](https://www.learncpp.com/)
- [Programming Tutorials - C, C++, OpenGL, STL](https://www.cprogramming.com/tutorial.html)
- Professional C++ (5th Edition), by Marc Gregoire

### OpenGL references

- [LearnOpenGL](https://learnopengl.com/) (Recommended OpenGL learning resource, it provides a lot of examples and explanations for OpenGL)
- [https://www.opengl.org/](https://www.opengl.org/)
- [OpenGL Step by Step](https://ogldev.org/)
- [Khronos OpenGL Wiki](https://wikis.khronos.org/opengl/Main_Page)
- OpenGL SuperBible (5th Edition), by Jr. Richard S. Wright, Nicholas Haemel, Graham Sellers, Benjamin Lipchak
- OpenGL, A Primer , by Edward Angel, Addison Wesley

### Other references
- [CMake Tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)
- [GLFW Documentation](https://www.glfw.org/docs/3.3/)
- [GLM Manual](https://github.com/g-truc/glm/blob/master/manual.md)
- [stb library](https://github.com/nothings/stb)
- [NameMC](https://namemc.com/minecraft-skins) (A good website to find Minecraft skins you like, you can download your favourite skin and add it to `assets/textures/` folder for your assignment)
- [Kenney](https://kenney.nl/) (A great source of free game assets)