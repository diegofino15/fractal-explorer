# Fractal Explorer

An optimized c++ program that lets you explore and zoom infinitely inside the well known fractals like Mandelbrot's or the Julia Set.

## Features

- Move with ZQSD inside the fractal
- Zoom with UP and DOWN arrows infinitely (until the long doubles don't have enough precision to continue...)
- Explore multiple sets including :
    - Mandelbrot Set
    - Julia Set
    - Burning ship
    - Tricorn
    - Phoenix

## Optimizations

The project is already very optimized, uses multithreading, and uses "old textures" so that the fractal isn't re-rendered at every frame.  

It uses a tile-based system : the screen is divided into 144 tiles (16*9), that are each rendered on their own thread. When a certain threashold of zoom or movement is reached, the tiles get re-rendered with the new camera position and zoom (while still showing the old texture to avoid black spots).

## Compiling

You need to have **raylib** installed and available on your PATH (you can install it via homebrew on MacOS), then you can run cmake build and the executable will compile.  
The CMakeLists.txt file contains code to compile another executable called **videogen**, it lets you generate videos of zooming inside the fractals (still in development).

## Flags

### Window and general settings
```
--fullscreen : Sets the app to fullscreen
--width [value] : Sets the width of the window (in pixels)
--height [value] : Sets the height of the window (in pixels)
--set [value] : Fractal to display (0 : Mandelbrot | 1 : Julia | 2 : Burning ship | 3 : Tricorn | 4 : Phoenix | 5 : Lyapunov | 6 : Mandelbrot with "light effect") (can change with O and P)
--it [value] : Sets the maximum number of iterations (can change with LEFT-ARROW and RIGHT-ARROW)
--fps [value] : Sets the target FPS
```

### Camera and zoom
```
--zoom [value] : Sets the zoom of the camera (can change with TOP-ARROW and BOTTOM-ARROW) (default: 500)
--x [value] : Sets the x position of the camera in world space (can change with Q and D)
--y [value] : Sets the y position of the camera in world space (can change with Z and S)
--speed [value] : Sets the speed of the camera (default: 500)
--zoom-speed [value] : Sets the speed of the zoom (default: 0.85)
```

### Advanced settings
```
--show-tiles : Shows the individual tiles (can toggle with LSHIFT)
--no-detached : Will not detach the threads, makes the app stutter but will show no visual glitches
--no-avoid-duplicates : Will not avoid unnecessary re-renders of the same tile, improves transitions but slows down the app a lot
--no-old-textures : Will make the app a lot faster but will show many visual glitches (black spots)
```

