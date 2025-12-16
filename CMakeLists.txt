cmake_minimum_required(VERSION 3.10)
project(SATRunner VERSION 3.0 LANGUAGES CXX)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<CONFIG:Release>:-O3>)
    add_compile_options($<$<CONFIG:Release>:-Wall>)
    add_compile_options($<$<CONFIG:Release>:-Wextra>)
endif()

include_directories(include)

# Find threads
find_package(Threads REQUIRED)

# Source files
file(GLOB SOURCES "src/*.cpp")

add_executable(sat-runner ${SOURCES})

target_link_libraries(sat-runner PRIVATE Threads::Threads)
