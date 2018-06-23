project(sdf-tests)

add_subdirectory("third_parties/googletest-release-1.8.0/")

include_directories("third_parties/googletest-release-1.8.0/include" "third_parties/googletest-release-1.8.0")

add_executable(run_tests tests.cpp)
target_link_libraries(run_tests gtest gtest_main)
