cmake_minimum_required(VERSION 3.18)

project(Craft LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
include(CTest)

# GLFW: window, OpenGL context, and input.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.10
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# GLAD 2: OpenGL 3.3 core loader for Windows and Linux.
# macOS uses its system OpenGL 3 core headers and framework.
if(NOT APPLE)
    FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/dav1dde/glad.git
        GIT_TAG v2.0.8
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR cmake
    )
    FetchContent_MakeAvailable(glad)

    glad_add_library(
        craft_glad_gl_core_33
        STATIC
        REPRODUCIBLE
        API gl:core=3.3
    )
endif()

# GLM: vector and matrix mathematics.
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

# Dear ImGui: runtime debug UI. Its sources are added in src/CMakeLists.txt.
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

# stb: image decoding and image-file writing.
# The upstream repository does not define a CMake target, so expose its
# single-header libraries through a small interface target.
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)

add_library(stb_headers INTERFACE)
add_library(stb::headers ALIAS stb_headers)
target_include_directories(stb_headers SYSTEM INTERFACE
    ${stb_SOURCE_DIR}
)

# FastNoise2 is optional: the authored whitebox levels do not use noise.
option(RELAY_USE_FASTNOISE "Build the starter's optional noise library" OFF)
if(RELAY_USE_FASTNOISE)
# FastNoise2: deterministic SIMD terrain generation for later exercises.
set(FASTNOISE2_TOOLS OFF CACHE BOOL "" FORCE)
set(FASTNOISE2_TESTS OFF CACHE BOOL "" FORCE)
set(FASTNOISE2_UTILITY OFF CACHE BOOL "" FORCE)
set(FASTNOISE2_STRICT_FP ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    fastnoise2
    GIT_REPOSITORY https://github.com/Auburn/FastNoise2.git
    GIT_TAG v1.1.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fastnoise2)

# FastNoise2/FastSIMD uses __regcall, which Apple Clang ignores on ARM64.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    if(TARGET FastSIMD_FastNoise)
        target_compile_options(FastSIMD_FastNoise PRIVATE -Wno-ignored-attributes)
    endif()
endif()

endif()
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
