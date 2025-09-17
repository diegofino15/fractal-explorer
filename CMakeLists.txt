cmake_minimum_required(VERSION 3.10.0)
project(videogen VERSION 0.1.0 LANGUAGES C CXX)
project(fractal-viewer VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Actual executable
add_executable(videogen src/videogen.cpp)
add_executable(fractal-viewer src/main.cpp)

# Add external libraries
target_include_directories(videogen PRIVATE include/external)

# Link packages
find_package(raylib CONFIG REQUIRED)
target_link_libraries(videogen PRIVATE raylib)
target_link_libraries(fractal-viewer PRIVATE raylib)