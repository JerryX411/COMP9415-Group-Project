add_executable(gameplay_tests gameplay_tests.cpp)
target_link_libraries(gameplay_tests PRIVATE craft_core)
add_test(NAME gameplay COMMAND gameplay_tests)

add_executable(showcase_tests showcase_tests.cpp)
target_link_libraries(showcase_tests PRIVATE craft_core)
add_test(NAME tutorial_showcase COMMAND showcase_tests)

add_executable(corona_tests corona_tests.cpp)
target_link_libraries(corona_tests PRIVATE craft_core)
add_test(NAME corona_run COMMAND corona_tests)

add_executable(surveillance_tests surveillance_tests.cpp)
target_link_libraries(surveillance_tests PRIVATE craft_core)
add_test(NAME surveillance_camera COMMAND surveillance_tests)

add_executable(helios_tests helios_tests.cpp)
target_link_libraries(helios_tests PRIVATE craft_core)
add_test(NAME helios_four_core_route COMMAND helios_tests)

add_executable(gate_tests gate_tests.cpp)
target_link_libraries(gate_tests PRIVATE craft_core)
add_test(NAME gate_hierarchy COMMAND gate_tests)
