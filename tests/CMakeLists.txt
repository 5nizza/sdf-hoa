# this project depends on several variables that are set by the external project
#   SDF_LIB_NAME
#   LIBS --- all third-party libs used by SDF_LIB_NAME library
#   THIRD_PARTIES

project(sdf-tests)

#add_subdirectory("${THIRD_PARTIES}/googletest-release-1.10.0")

include_directories("${THIRD_PARTIES}/googletest-release-1.10.0/googletest/include" "${THIRD_PARTIES}/googletest-release-1.10.0/googletest")

add_executable(test_synt tests_synt.cpp)
target_link_libraries(test_synt "${SDF_LIB_NAME}" "${LIBS}" gtest gtest_main)

add_executable(test_reg_translator test_reg_translator.cpp)
target_link_libraries(test_reg_translator "${SDF_LIB_NAME}" "${LIBS}" gtest gtest_main)

add_executable(test_graph test_graph.cpp)
target_link_libraries(test_graph "${SDF_LIB_NAME}" "${LIBS}" gtest gtest_main)

file(COPY ./specs DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# for some reason this command does not work for me
# (it is supposed to be better than the above: the above command only copies once, at the configuration time)
#add_custom_command(TARGET run_tests PRE_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_directory
#        ${CMAKE_SOURCE_DIR}/specs $<TARGET_FILE_DIR:run_tests>)


