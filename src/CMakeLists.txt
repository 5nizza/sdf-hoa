add_definitions(-DSYSTEMTYPE="Linux")

file(STRINGS extrasources.make project_FILES)   # AK: WHY this way?
foreach(fil ${project_FILES})
  string(REPLACE " " "" fil ${fil})
  if(NOT ${fil} MATCHES main.cpp)
   list(APPEND project_SRC ${fil})
  endif(NOT ${fil} MATCHES main.cpp)
endforeach(fil)


include_directories("${SPDLOG_INCLUDE_DIR}")
include_directories("${AIGER_INCLUDE_DIR}")
include_directories("${CUDD_INCLUDE_DIR}")

# set the default build type to 'debug'.
SET(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)

#add_library(${PROJECT}
#            STATIC
#            ${project_SRC} ArgsParser.h)

#target_link_libraries(${PROJECT} ${CUDD_LIBRARIES})

#set_target_properties(${PROJECT} PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC -DNDEBUG -O3")

add_executable(${PROJECT}-opt ${project_SRC} main)
target_link_libraries(${PROJECT}-opt ${AIGER_LIBRARIES})
target_link_libraries(${PROJECT}-opt ${CUDD_LIBRARIES})
target_link_libraries(${PROJECT}-opt bddx spot)
set_target_properties(${PROJECT}-opt PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC -DNDEBUG -O3")

add_executable(${PROJECT}-debug ${project_SRC} main k_reduce.cpp)
target_link_libraries(${PROJECT}-debug ${AIGER_LIBRARIES})
target_link_libraries(${PROJECT}-debug ${CUDD_LIBRARIES})
target_link_libraries(${PROJECT}-debug bddx spot)
set_target_properties(${PROJECT}-debug PROPERTIES COMPILE_FLAGS "${COMPILE_FLAGS} -fPIC")

