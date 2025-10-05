#pragma once
#include <raylib.h>

// Mandelbrot
Color getColorFromPoint_Mandelbrot(long double a, long double b, float maxIterations);
Color getColorFromPoint_Mandelbrot_LightEffect(long double a, long double b, float maxIterations);

// Julia
Color getColorFromPoint_Julia(long double a, long double b, float maxIterations);

// Burning ship
Color getColorFromPoint_BurningShip(long double a, long double b, int maxIterations);

// Tricorn
Color getColorFromPoint_Tricorn(long double a, long double b, int maxIterations);

// Phoenix
Color getColorFromPoint_Phoenix(long double a, long double b, int maxIterations);

// Lyapunov
Color getColorFromPoint_Lyapunov(long double a, long double b, int maxIterations);


