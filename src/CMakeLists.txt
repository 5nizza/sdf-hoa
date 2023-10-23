list(APPEND SRC_FILES
        "k_reduce.cpp"
        "game_solver.cpp"
        "synthesizer.cpp"
        "ltl_parser.cpp"
        "ehoa_parser.cpp"
        "utils.cpp"
        "logger_instance.cpp"
        )

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


# build common oject files into a 'virtual library'
add_library(sdf-object-library OBJECT ${SRC_FILES})

# sdf-tlsf
add_executable(sdf-tlsf main.cpp $<TARGET_OBJECTS:sdf-object-library>)
target_link_libraries(sdf-tlsf "${LIBS}")

# sdf-hoa
add_executable(sdf-hoa main_hoa.cpp $<TARGET_OBJECTS:sdf-object-library>)
target_link_libraries(sdf-hoa "${LIBS}")

# static library
add_library(${SDF_LIB_NAME} STATIC $<TARGET_OBJECTS:sdf-object-library>)
#add_library(${SDF_LIB_NAME} SHARED $<TARGET_OBJECTS:sdf-object-library>)
