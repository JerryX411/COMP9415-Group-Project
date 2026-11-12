# Sources are collected by directory; CONFIGURE_DEPENDS re-runs the glob at
# build time, so adding a .cpp file needs no CMake edit.
#   core/, graphics/ -> craft_core (library, also linked by tests)
#   app/, main.cpp   -> Craft (executable, needs ImGui)
file(GLOB_RECURSE CRAFT_CORE_SOURCES CONFIGURE_DEPENDS
    core/*.cpp
    graphics/*.cpp
)
file(GLOB_RECURSE CRAFT_APP_SOURCES CONFIGURE_DEPENDS
    app/*.cpp
)

# SceneBenchmark has no ImGui dependency, so it lives in the library.
set(CRAFT_BENCHMARK_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/app/SceneBenchmark.cpp)
list(REMOVE_ITEM CRAFT_APP_SOURCES ${CRAFT_BENCHMARK_SOURCE})

add_library(craft_core STATIC
    ${CRAFT_CORE_SOURCES}
    ${CRAFT_BENCHMARK_SOURCE}
)

target_include_directories(craft_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(craft_core
    PUBLIC
        glm::glm
    PRIVATE
        glfw
        stb::headers
)
if(RELAY_USE_FASTNOISE)
    target_link_libraries(craft_core PRIVATE FastNoise2)
endif()
if(WIN32)
    target_link_libraries(craft_core PRIVATE imm32 user32)
endif()

target_compile_definitions(craft_core PRIVATE
    CRAFT_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets"
)

if(APPLE)
    target_link_libraries(craft_core PRIVATE "-framework OpenGL")
    target_compile_definitions(craft_core PRIVATE GL_SILENCE_DEPRECATION)
else()
    # PUBLIC because the starter's main.cpp includes graphics/OpenGL.h.
    target_link_libraries(craft_core PUBLIC craft_glad_gl_core_33)
endif()

set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(Craft
    main.cpp
    ${CRAFT_APP_SOURCES}
    ${IMGUI_SOURCES}
)

target_include_directories(Craft PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(Craft PRIVATE
    craft_core
    glfw
    stb::headers
)
if(UNIX AND NOT APPLE)
    target_link_libraries(Craft PRIVATE ${CMAKE_DL_LIBS} pthread m)
endif()

# Keep the existing Pixi command (`build/Craft`) stable after adding src/.
set_target_properties(Craft PROPERTIES
    OUTPUT_NAME CoreExplorer
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)

# Refresh runtime assets even when an asset-only edit does not relink the game.
get_property(CRAFT_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(CRAFT_RUNTIME_ASSETS "${PROJECT_BINARY_DIR}/assets")
if(CRAFT_MULTI_CONFIG)
    set(CRAFT_RUNTIME_ASSETS "${PROJECT_BINARY_DIR}/$<CONFIG>/assets")
endif()
add_custom_target(craft_runtime_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/assets" "${CRAFT_RUNTIME_ASSETS}"
    COMMENT "Copying runtime assets"
    VERBATIM
)
add_dependencies(Craft craft_runtime_assets)

# Optional local authoring tools; source distributions work without this directory.
option(BUILD_MAP_EDITOR "Build the local-only map authoring tool" OFF)
if(BUILD_MAP_EDITOR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/editor/CMakeLists.txt")
    add_subdirectory(editor)
endif()
