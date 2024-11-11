list(APPEND SRC_FILES
        "k_reduce.cpp"
        "game_solver.cpp"
        "synthesizer.cpp"
        "ltl_parser.cpp"
        "ehoa_parser.cpp"
        "utils.cpp"
        "bdd_visitor.cpp"
        "generate_qcir.cpp"
        "dump_det_as_bdd.cpp"
        )

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


# build common oject files into a 'virtual library'
add_library(sdf-object-library OBJECT ${SRC_FILES})

# generate-qcir
add_executable(generate-qcir main_generate_qcir.cpp $<TARGET_OBJECTS:sdf-object-library>)
target_link_libraries(generate-qcir "${LIBS}")

# static library
add_library(${SDF_LIB_NAME} STATIC $<TARGET_OBJECTS:sdf-object-library>)
#add_library(${SDF_LIB_NAME} SHARED $<TARGET_OBJECTS:sdf-object-library>)
