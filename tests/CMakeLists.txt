# Game regressions are independent of OpenGL contexts and external test downloads.
add_executable(relay_tests relay_tests.cpp)
target_link_libraries(relay_tests PRIVATE craft_core)
foreach(case movement jump low_barrier wall trap_air damage_cooldown switch pickup gate timeout reset particles hierarchy movers routes camera sprint death knockback sensitivity model_bounds input_focus control_feel jump_buffer coyote frame_rates trap_edges rounded_collision mover_parts pose_transitions expressive_bounds animation_events foot_plant turn_body weighted_impact arrival crouch crouch_toggle crouch_tunnel level_transition checkpoint checkpoint_solid_return timeout_no_respawn key_bindings hit_sources slide lives slide_cancel speed_pad pressure_door wall_bounce wall_jump slide_tricks training training_reboots pad_modes tunnel_camera powerups wall_camera camera_steady time_pickup camera_edge level_files crimson_tile jetpack portals upper_floor practice wall_action)
    add_test(NAME relay_${case} COMMAND relay_tests ${case})
endforeach()
add_test(NAME relay_audio_mix COMMAND Craft --audio-demo ${CMAKE_CURRENT_BINARY_DIR}/audio-mix.wav)
add_test(NAME relay_audio_fallback COMMAND Craft --audio-demo ${CMAKE_CURRENT_BINARY_DIR}/audio-fallback.wav)
set_tests_properties(relay_audio_fallback PROPERTIES ENVIRONMENT "CRAFT_AUDIO_CC0_ONLY=1")
if(WIN32)
    add_test(NAME relay_native_input COMMAND Craft --input-self-test)
    # Opens the sound device and loads every effect (needs an audio device).
    add_test(NAME relay_audio COMMAND Craft --audio-check)
endif()
option(RELAY_STARTER_TESTS "Also build the original starter Catch2 tests" OFF)
if(RELAY_STARTER_TESTS)
# Optional self-checks only: tests are not an assignment deliverable or rubric item.
# Catch2 belongs to the test build only; production targets do not depend on it.
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.15.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

# Any test_*.cpp added here is picked up automatically (see src/CMakeLists.txt).
file(GLOB CRAFT_TEST_SOURCES CONFIGURE_DEPENDS
    test_*.cpp
)

add_executable(craft_tests
    ${CRAFT_TEST_SOURCES}
)

target_link_libraries(craft_tests PRIVATE
    craft_core
    Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(craft_tests
    DISCOVERY_MODE PRE_TEST
)
endif()
