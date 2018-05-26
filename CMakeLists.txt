cmake_minimum_required(VERSION 2.6)
project(sdf-hoa)
set(PROJECT sdf-hoa)
set(VERSION 0.0.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14 -pthread")

set(THIRD_PARTIES "${PROJECT_SOURCE_DIR}/third_parties")

set(SPDLOG_INCLUDE_DIR    "${THIRD_PARTIES}/spdlog/")

set(AIGER_INCLUDE_DIR    "${THIRD_PARTIES}/aiger-1.9.4/")
set(AIGER_LIBRARIES      "${THIRD_PARTIES}/aiger-1.9.4/aiger.o")

# we need this mtr header, that is why we reference cudd using this hack
# (instead of using configure --prefix && make && make install, which does not instll mtr header)
set(CUDD_INCLUDE_DIR   "${THIRD_PARTIES}/cudd-3.0.0/cudd/" "${THIRD_PARTIES}/cudd-3.0.0/cplusplus/" "${THIRD_PARTIES}/cudd-3.0.0/mtr/")
set(CUDD_LIBRARIES   "${THIRD_PARTIES}/cudd-3.0.0/cudd/.libs/libcudd.a")

if(EXISTS "${THIRD_PARTIES}/spot-install-prefix/")
    message("I am going to use locally installed spot.")
    include_directories("${THIRD_PARTIES}/spot-install-prefix/include/")
    link_directories("${THIRD_PARTIES}/spot-install-prefix/lib/")
endif()

add_subdirectory(src)

enable_testing()
add_subdirectory(func_tests)
