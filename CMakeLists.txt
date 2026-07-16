cmake_minimum_required(VERSION 3.20)

project(super_cmake
    VERSION 0.0.1
    DESCRIPTION "CMake port of kingbirdogd/super_make (common.mk)"
    LANGUAGES C CXX)

# Make the SuperCMake module (the common.mk equivalent) available everywhere.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SuperCMake)

# Establish the default configuration and the bin/ lib64/ output tree.
super_init()

# Build the matching engine project.
add_subdirectory(matching_engine)
