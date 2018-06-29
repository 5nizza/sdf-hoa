# this project depends on several variables that are set by the external project
#   SDF_LIB_NAME
#   LIBS --- all third-party libs used by SDF_LIB_NAME library

project(sdf-tests)

add_subdirectory("third_parties/googletest-release-1.8.0/")

include_directories("third_parties/googletest-release-1.8.0/include" "third_parties/googletest-release-1.8.0")

add_executable(run_tests tests.cpp)
target_link_libraries(run_tests "${SDF_LIB_NAME}" "${LIBS}" gtest gtest_main)

file(COPY ./specs DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# for some reason this command does not work for me
# (it is supposed to be better than the above: the above command only copies once, at the configuration time)
#add_custom_command(TARGET run_tests PRE_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_directory
#        ${CMAKE_SOURCE_DIR}/specs $<TARGET_FILE_DIR:run_tests>)


