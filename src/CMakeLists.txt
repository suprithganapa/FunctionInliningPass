cmake_minimum_required(VERSION 3.15)
project(FunctionInliningPass)

# Find the LLVM package installed on your system
find_package(LLVM REQUIRED CONFIG)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Set up include and link directories based on what find_package found
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Enforce C++17 and disable RTTI (required by LLVM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

# Create the shared library (plugin)
add_library(InlinePass MODULE InlinePass.cpp)
target_link_libraries(InlinePass PUBLIC LLVM)
