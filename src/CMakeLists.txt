file(STRINGS extrasources.make project_FILES)   # use cmake's glob?
foreach(fil ${project_FILES})
  string(REPLACE " " "" fil ${fil})
  if(NOT ${fil} MATCHES main.cpp)
   list(APPEND project_SRC ${fil})
  endif(NOT ${fil} MATCHES main.cpp)
endforeach(fil)


# set the default build type to 'debug'.
SET(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)

add_executable(${PROJECT_NAME}-opt ${project_SRC} main)
target_link_libraries(${PROJECT_NAME}-opt "${LIBS}")
set_target_properties(${PROJECT_NAME}-opt PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC -DNDEBUG -O3")

add_executable(${PROJECT_NAME}-debug ${project_SRC} main)
target_link_libraries(${PROJECT_NAME}-debug "${LIBS}")
set_target_properties(${PROJECT_NAME}-debug PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC")

add_library(${SDF_LIB_NAME} SHARED ${project_SRC})
