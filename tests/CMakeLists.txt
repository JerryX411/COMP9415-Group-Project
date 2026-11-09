add_executable(lite_tests lite_tests.cpp)
target_link_libraries(lite_tests PRIVATE craft_core)
add_test(NAME lite_gameplay COMMAND lite_tests)

add_executable(showcase_tests showcase_tests.cpp)
target_link_libraries(showcase_tests PRIVATE craft_core)
add_test(NAME tutorial_showcase COMMAND showcase_tests)

add_executable(corona_tests corona_tests.cpp)
target_link_libraries(corona_tests PRIVATE craft_core)
add_test(NAME corona_run COMMAND corona_tests)
