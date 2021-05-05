file(STRINGS extrasources.make project_FILES)   # use cmake's glob?
foreach(fil ${project_FILES})
  string(REPLACE " " "" fil ${fil})
  if(NOT (${fil} MATCHES main.cpp OR ${fil} MATCHES main_hoa.cpp))
    list(APPEND project_SRC ${fil})
  endif()
endforeach(fil)


# set the default build type to 'debug'.
SET(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)

# main (TLSF input format)
add_executable       (sdf-tlsf-opt ${project_SRC} main)
target_link_libraries(sdf-tlsf-opt "${LIBS}")
set_target_properties(sdf-tlsf-opt PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC -DNDEBUG -O3")

add_executable       (sdf-tlsf-debug ${project_SRC} main)
target_link_libraries(sdf-tlsf-debug "${LIBS}")
set_target_properties(sdf-tlsf-debug PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC")

# main (HOA input format)
add_executable       (sdf-hoa-opt ${project_SRC} main_hoa)
target_link_libraries(sdf-hoa-opt "${LIBS}")
set_target_properties(sdf-hoa-opt PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC -DNDEBUG -O3")

add_executable       (sdf-hoa-debug ${project_SRC} main_hoa)
target_link_libraries(sdf-hoa-debug "${LIBS}")
set_target_properties(sdf-hoa-debug PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC")

# shared library
add_library(${SDF_LIB_NAME} SHARED ${project_SRC})
