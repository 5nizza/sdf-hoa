list(APPEND SRC_FILES
        "k_reduce.cpp"
        "game_solver.cpp"
        "synthesizer.cpp"
        "ltl_parser.cpp"
        "ehoa_parser.cpp"
        "utils.cpp"
        "logger_instance.cpp"
        "graph.cpp"
        "graph_algo.cpp"
        "reg/reg_reduction.cpp"
        "reg/ord_partition.cpp"
        "reg/lin_order_domain.cpp"
        )

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable       (tlsf-sdf ${SRC_FILES} main.cpp)
target_link_libraries(tlsf-sdf "${LIBS}")

add_executable       (hoa-sdf ${SRC_FILES} main_hoa.cpp)
target_link_libraries(hoa-sdf "${LIBS}")

add_executable       (reg-sdf ${SRC_FILES} main_reg.cpp)
target_link_libraries(reg-sdf "${LIBS}")

add_executable       (reg-translator ${SRC_FILES} main_translator.cpp)
target_link_libraries(reg-translator "${LIBS}")

add_executable       (tmp ${SRC_FILES} main_tmp.cpp)
target_link_libraries(tmp "${LIBS}")

# shared library
add_library(${SDF_LIB_NAME} SHARED ${SRC_FILES})
