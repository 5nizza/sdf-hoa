cmake_minimum_required(VERSION 3.0)
project(sdf VERSION 0.0.0)

set(SDF_LIB_NAME sdf)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14 -pthread")

set(THIRD_PARTIES "${PROJECT_SOURCE_DIR}/third_parties")

include_directories(src)
include_directories("${THIRD_PARTIES}/spdlog/")
include_directories("${THIRD_PARTIES}/aiger-1.9.4/")
# for cudd, we need the mtr header, that is why we reference cudd using this hack
# (instead of using expected `configure --prefix && make && make install`, which does not install the mtr header)
include_directories("${THIRD_PARTIES}/cudd-3.0.0/cudd/"
                    "${THIRD_PARTIES}/cudd-3.0.0/cplusplus/"
                    # to be able to include "cuddInt.h" that includes a lot...
                    "${THIRD_PARTIES}/cudd-3.0.0/epd/"
                    "${THIRD_PARTIES}/cudd-3.0.0/st/"
                    "${THIRD_PARTIES}/cudd-3.0.0/mtr/"
                    "${THIRD_PARTIES}/cudd-3.0.0/")
include_directories("${THIRD_PARTIES}/args-6.2.3/")
include_directories("${THIRD_PARTIES}/pstreams-1.0.3/")

list(APPEND LIBS "${THIRD_PARTIES}/cudd-3.0.0/cudd/.libs/libcudd.a")
list(APPEND LIBS "${THIRD_PARTIES}/aiger-1.9.4/aiger.o")

if(EXISTS "${THIRD_PARTIES}/spot-install-prefix/")
    message("\n ********* I am going to use locally installed spot (and link it statically) *********** \n")
    include_directories("${THIRD_PARTIES}/spot-install-prefix/include/")
    list(APPEND LIBS "${THIRD_PARTIES}/spot-install-prefix/lib/libspot.a")
    list(APPEND LIBS "${THIRD_PARTIES}/spot-install-prefix/lib/libbddx.a")
else()
    list(APPEND LIBS "spot" "bddx")
endif()

add_subdirectory(src)
add_subdirectory(tests)

SET(BUILD_GMOCK OFF CACHE BOOL "")  # do not build gmock
SET(INSTALL_GTEST OFF CACHE BOOL "") # do not install googletest
add_subdirectory(${THIRD_PARTIES}/googletest-release-1.10.0/)

enable_testing()
