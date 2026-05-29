cmake_minimum_required(VERSION 3.16)
project(FunctionInliningPass VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Build an out-of-tree New PM pass plugin compatible with `opt -load-pass-plugin`.
add_llvm_pass_plugin(FunctionInliningPass
  src/InlinerPass.cpp
)

set_target_properties(FunctionInliningPass PROPERTIES
  PREFIX "lib"
  OUTPUT_NAME "FunctionInliningPass"
)
