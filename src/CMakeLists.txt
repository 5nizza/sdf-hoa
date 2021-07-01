list(APPEND SRC_FILES
        "k_reduce.cpp"
        "game_solver.cpp"
        "synthesizer.cpp"
        "ltl_parser.cpp"
        "ehoa_parser.cpp"
        "utils.cpp"
        "reg_reduction.cpp"
        "partition_tst_asgn.cpp"
        "logger_instance.cpp"
        "graph_algo.cpp"
        )

add_executable       (tlsf-sdf ${SRC_FILES} main)
target_link_libraries(tlsf-sdf "${LIBS}")

add_executable       (hoa-sdf ${SRC_FILES} main_hoa)
target_link_libraries(hoa-sdf "${LIBS}")

add_executable       (reg-sdf ${SRC_FILES} main_reg)
target_link_libraries(reg-sdf "${LIBS}")

add_executable       (reg-translator ${SRC_FILES} main_translator)
target_link_libraries(reg-translator "${LIBS}")

add_executable       (tmp ${SRC_FILES} main_tmp)
target_link_libraries(tmp "${LIBS}")

# shared library
add_library(${SDF_LIB_NAME} SHARED ${SRC_FILES})
