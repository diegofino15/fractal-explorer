cmake_minimum_required(VERSION 3.10.0)
project(new-mandelbrot-fractal VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Fill with packages
find_package(raylib CONFIG REQUIRED)

add_executable(new-mandelbrot-fractal main.cpp)

# Link packages
target_link_libraries(new-mandelbrot-fractal PRIVATE raylib)