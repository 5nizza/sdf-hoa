list(APPEND SRC_FILES
        "k_reduce.cpp"
        "game_solver.cpp"
        "synthesizer.cpp"
        "ltl_parser.cpp"
        "ehoa_parser.cpp"
        "utils.cpp"
        )


# set the default build type to 'debug'.
SET(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)

# main (TLSF input format)
add_executable       (sdf-tlsf-opt ${SRC_FILES} main)
target_link_libraries(sdf-tlsf-opt "${LIBS}")
set_target_properties(sdf-tlsf-opt PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC -DNDEBUG -O3")

add_executable       (sdf-tlsf-debug ${SRC_FILES} main)
target_link_libraries(sdf-tlsf-debug "${LIBS}")
set_target_properties(sdf-tlsf-debug PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC")

# main (HOA input format)
add_executable       (sdf-hoa-opt ${SRC_FILES} main_hoa)
target_link_libraries(sdf-hoa-opt "${LIBS}")
set_target_properties(sdf-hoa-opt PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC -DNDEBUG -O3")

add_executable       (sdf-hoa-debug ${SRC_FILES} main_hoa)
target_link_libraries(sdf-hoa-debug "${LIBS}")
set_target_properties(sdf-hoa-debug PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC")

# shared library
add_library(${SDF_LIB_NAME} SHARED ${SRC_FILES})
