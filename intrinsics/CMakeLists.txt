cmake_minimum_required(VERSION 3.21)
project(intrinsics)

SET (CMAKE_CXX_COMPILER "clang++")

set(HELPER_CLANG_FLAGS
    -std=c++17
    -O3
    -Iremill/include
    -Wno-gnu-inline-cpp-without-extern
    -fno-discard-value-names
    -fstrict-aliasing
    -fno-slp-vectorize
    -mllvm
    -enable-tbaa=true
    -emit-llvm
)

set(SOURCES "main.cpp")

add_custom_command(OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/vmprotect64.ll"
    "${CMAKE_CURRENT_BINARY_DIR}/vmprotect32.ll"
    COMMAND "${CMAKE_CXX_COMPILER}" ${HELPER_CLANG_FLAGS} -DADDRESS_SIZE_BITS=64 -m64 -S main.cpp -o vmprotect32.ll
    COMMAND "${CMAKE_CXX_COMPILER}" ${HELPER_CLANG_FLAGS} -DADDRESS_SIZE_BITS=64 -m64 -S main.cpp -o vmprotect64.ll
    MAIN_DEPENDENCY ${SOURCES}
    DEPENDS ${SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_target(${PROJECT_NAME} ALL
    DEPENDS vmprotect64.ll vmprotect32.ll
    SOURCES ${SOURCES}
)
