cmake_minimum_required(VERSION 3.10.0)
project(fractal-project VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find raylib
find_package(raylib CONFIG REQUIRED)

# Shared library for common code (sets-definition)
add_library(fractal_common src/sets_definition.cpp)

# Add include directories for the common library
target_include_directories(fractal_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link raylib
target_link_libraries(fractal_common PUBLIC raylib)

# Executables
add_executable(fractal-viewer src/fractal-viewer.cpp)
add_executable(videogen src/videogen.cpp)

# Link against common library + raylib
target_link_libraries(fractal-viewer PRIVATE fractal_common)
target_link_libraries(videogen PRIVATE fractal_common)

# If you need external includes specifically for videogen
target_include_directories(videogen PRIVATE include/external)