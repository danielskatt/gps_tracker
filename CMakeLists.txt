# Find Zephyr. This also loads Zephyr's build system.
cmake_minimum_required(VERSION 3.13.1)

# Set default board
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gps_tracker)

# Add include path for #include statements for submodules/.../xx.h
include_directories(.)

# ---------------------------------------------------------------------------------------
# Build targets
# ---------------------------------------------------------------------------------------
target_sources(app PRIVATE
    src/main.c
)

add_subdirectory(src)
add_subdirectory(drivers)
