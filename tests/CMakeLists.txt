add_executable(lite_tests lite_tests.cpp)
target_link_libraries(lite_tests PRIVATE craft_core)
add_test(NAME lite_gameplay COMMAND lite_tests)
