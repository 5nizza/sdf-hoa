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
add_executable       (tlsf-sdf-opt ${SRC_FILES} main)
target_link_libraries(tlsf-sdf-opt "${LIBS}")
set_target_properties(tlsf-sdf-opt PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC -DNDEBUG -O3")

add_executable       (tlsf-sdf-debug ${SRC_FILES} main)
target_link_libraries(tlsf-sdf-debug "${LIBS}")
set_target_properties(tlsf-sdf-debug PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC")

# main (HOA input format)
add_executable       (hoa-sdf-opt ${SRC_FILES} main_hoa)
target_link_libraries(hoa-sdf-opt "${LIBS}")
set_target_properties(hoa-sdf-opt PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC -DNDEBUG -O3")

add_executable       (hoa-sdf-debug ${SRC_FILES} main_hoa)
target_link_libraries(hoa-sdf-debug "${LIBS}")
set_target_properties(hoa-sdf-debug PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC")

# main (REGISTERS)
add_executable       (reg-sdf-opt ${SRC_FILES} main_reg)
target_link_libraries(reg-sdf-opt "${LIBS}")
set_target_properties(reg-sdf-opt PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC -DNDEBUG -O3")

add_executable       (reg-sdf-debug ${SRC_FILES} main_reg)
target_link_libraries(reg-sdf-debug "${LIBS}")
set_target_properties(reg-sdf-debug PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC")

# shared library
add_library(${SDF_LIB_NAME} SHARED ${SRC_FILES})
