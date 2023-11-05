cmake_minimum_required(VERSION 3.15)

project(titan)

# Packages
find_package(fmt CONFIG REQUIRED)
find_package(LLVM 15.0 CONFIG REQUIRED)
find_package(triton CONFIG REQUIRED)
find_package(range-v3 CONFIG REQUIRED)

file(GLOB_RECURSE TITAN_SOURCES  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE TITAN_INCLUDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${TITAN_SOURCES} ${TITAN_INCLUDES})

add_executable(${PROJECT_NAME} ${TITAN_SOURCES} ${TITAN_INCLUDES})

target_include_directories(${PROJECT_NAME} PRIVATE
  "src/"
)
target_compile_features(${PROJECT_NAME} PRIVATE 
  cxx_std_20
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
  fmt::fmt
  range-v3 
  triton::triton
)

add_subdirectory(intrinsics)
