# this project depends on several variables that are set by the external project
#   SDF_LIB_NAME
#   LIBS --- all third-party libs used by SDF_LIB_NAME library

project(sdf-tests)

add_subdirectory("third_parties/googletest-release-1.8.0/")

include_directories("third_parties/googletest-release-1.8.0/include" "third_parties/googletest-release-1.8.0")

add_executable(run_tests tests.cpp)
target_link_libraries(run_tests "${SDF_LIB_NAME}" "${LIBS}" gtest gtest_main)

file(COPY ./specs DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
