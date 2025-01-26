cmake_minimum_required(VERSION 3.30)
project(yai VERSION 0.1 LANGUAGES C CXX)

include(cmake/CPM.cmake)

cpmaddpackage("gh:gcca/xai-cpp#master")

find_package(Python3 COMPONENTS Development.Module REQUIRED)
find_package(Boost COMPONENTS json REQUIRED)
find_package(PostgreSQL REQUIRED)

set(COMMON_COMPILE_OPTIONS
    -Wall
    -Wextra
    -Werror
    -Wpedantic
    -Wshadow
    -Weverything
    -Wconversion
    -Wsign-conversion
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wfloat-equal
    -Wformat=2
    -Wnull-dereference
    -Wundef
    -Wuninitialized
    -Wcast-align
    -Wformat-security
    -Wstrict-overflow
    -Wswitch-enum
    -Wunused-variable
    -Wunused-parameter
    -Wpointer-arith
    -Wcast-align
    -Wno-variadic-macros
    -fexceptions
    -fsafe-buffer-usage-suggestions
    -Wno-c++98-compat
    -Wno-c++98-compat-pedantic
    -Wno-padded
    -Wno-covered-switch-default
    -Wno-unsafe-buffer-usage)

set(ABI_COMPILE_OPTIONS ${COMMON_COMPILE_OPTIONS}
  -Wno-disabled-macro-expansion
  -Wno-cast-function-type-strict
  -Wno-missing-prototypes)

function(add_abi name)
  Python3_add_library(${name} MODULE ${ARGN} ${CMAKE_SOURCE_DIR}/yai-core/pyAi.cpp)
  target_link_libraries(${name} PRIVATE PostgreSQL::PostgreSQL Boost::json xAI::xAI)
  target_compile_options(${name} PRIVATE ${ABI_COMPILE_OPTIONS} -I${CMAKE_SOURCE_DIR}/yai-core)
endfunction()

add_subdirectory(yai-core)
add_subdirectory(yai-booking)
add_subdirectory(yai-chat)
