#include "raylib.h"
#include <thread>


// Constants
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
// What set to display | 0 : Mandebrot | 1 : Julia | 2 : Burning ship | 3 : Tricorn | 4 : Phoenix
const int SET = 0;

// Detaches the threads, makes the app smoother but can cause a lot of visual glitches at high iterations and big zoom
const bool FAST_MODE = false;
// Should remove black screens, but slows down the app
const bool USE_OLD_TEXTURES = true;

// How many horizontal and vertical threads to create
const int partsX = 16;
const int partsY = 9;
// Debug tool to visualize the individual threads
const bool SHOW_PARTS = false;

// What change in zoom should trigger a re-render of the view (0.5 -> 50%)
const float zoomAceptedChange = 0.25f;
// What change in position should trigger a re-render of the view
const float cameraAceptedChange = 0.5f;

// Camera
long double cameraX = 0;
long double cameraY = 0;
const float cameraSpeed = 500.0f;
long double zoom = 500;
const float zoomSpeed = 1.5f;
float maxIterations = 100;
const int partWidth = SCREEN_WIDTH / partsX;
const int partHeight = SCREEN_HEIGHT / partsY;



// DEFINITION OF THE SETS //

// Mandelbrot set
const long double pisqrtpi = PI * sqrt(PI);
Color getColorFromPoint_Mandelbrot(long double a, long double b, float maxIterations) {
  long double ca = a;
  long double cb = b;
  int n;
  long double aa, bb;
  for (n = 0; (a * a + b * b <= 16) && (n < maxIterations); n++) {
    aa = a * a - b * b + ca;
    bb = 2.0 * a * b + cb;
    a = aa;
    b = bb;
  }

  if (n >= maxIterations) return BLACK;

  // --- Smooth coloring ---
  float zn = sqrt(a * a + b * b);
  float smooth = n + 1 - log(log(zn)) / log(2.0);
  float t = smooth / maxIterations; // Normalized [0..1]

  // --- Orange/Yellow gradient ---
  unsigned char r = (unsigned char)(255 * t);                      // Full red
  unsigned char g = (unsigned char)(200 * sqrt(t));                // Green increases slowly
  unsigned char bl = (unsigned char)(30 * (1.0 - t));              // Low blue for warmth

  return Color{r, g, bl, 255};
}

// Julia Set
const long double julia_ca = -0.7;    // Real part of c
const long double julia_cb = 0.27015; // Imaginary part of c
Color HSVtoRGB(float h, float s, float v) {
  float r, g, b;

  int i = int(h * 6);
  float f = h * 6 - i;
  float p = v * (1 - s);
  float q = v * (1 - f * s);
  float t = v * (1 - (1 - f) * s);

  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }

  return Color{
    (unsigned char)(r * 255),
    (unsigned char)(g * 255),
    (unsigned char)(b * 255),
    255
  };
}
Color getColorFromPoint_Julia(long double a, long double b, float maxIterations) {
  int n = 0;
  long double aa, bb;

  for (; n < maxIterations; ++n) {
    if ((a*a + b*b) > 4.0) break;
    aa = a * a - b * b + julia_ca;
    bb = 2.0 * a * b + julia_cb;
    a = aa;
    b = bb;
  }

  if (n == maxIterations) return BLACK; // Inside set

  // Smooth coloring
  long double zn = sqrt(a*a + b*b);
  long double smooth = n + 1 - log2(log2(zn));

  float hue = (float)(0.95f + 20.0f * smooth / maxIterations); // tweak multiplier
  hue = fmod(hue, 1.0f); // keep hue in [0,1]
  float saturation = 0.8f;
  float value = 1.0f;

  return HSVtoRGB(hue, saturation, value);
}

// Burning ship
Color getColorFromPoint_BurningShip(long double a, long double b, int maxIterations) {
    long double x = 0, y = 0;
    int n = 0;
    while (x*x + y*y <= 4 && n < maxIterations) {
        long double xtemp = x*x - y*y + a;
        y = fabs(2 * x * y) + b;
        x = fabs(xtemp);
        n++;
    }

    float t = (float)n / maxIterations;
    return (n == maxIterations) ? BLACK : Color{(unsigned char)(9*(1-t)*t*t*t*255),
                                                 (unsigned char)(15*(1-t)*(1-t)*t*t*255),
                                                 (unsigned char)(8.5*(1-t)*(1-t)*(1-t)*t*255), 255};
}

// Tricorn
Color getColorFromPoint_Tricorn(long double a, long double b, int maxIterations) {
    long double x = 0, y = 0;
    int n = 0;
    while (x*x + y*y <= 4 && n < maxIterations) {
        long double xtemp = x*x - y*y + a;
        y = -2 * x * y + b;
        x = xtemp;
        n++;
    }

    float t = (float)n / maxIterations;
    return (n == maxIterations) ? BLACK : Color{(unsigned char)(255*t),
                                                 (unsigned char)(255*(1-t)),
                                                 (unsigned char)(128*t), 255};
}

// Phoenix
Color getColorFromPoint_Phoenix(long double a, long double b, int maxIterations) {
    // Complex parameters
    long double cRe = a;
    long double cIm = b;

    // Phoenix constant p (can be tweaked for different visuals)
    const long double pRe = -0.5;
    const long double pIm = 0.0;

    long double x = 0.0, y = 0.0;      // z_n
    long double xPrev = 0.0, yPrev = 0.0; // z_{n-1}
    
    int n = 0;
    while ((x*x + y*y <= 4.0) && n < maxIterations) {
        // Complex multiplication: z_n^2
        long double x2 = x*x - y*y;
        long double y2 = 2*x*y;

        // Add c and p * z_{n-1}
        long double xTemp = x2 + cRe + (pRe * xPrev - pIm * yPrev);
        long double yTemp = y2 + cIm + (pRe * yPrev + pIm * xPrev);

        xPrev = x;
        yPrev = y;
        x = xTemp;
        y = yTemp;

        n++;
    }

    if (n == maxIterations) return BLACK;

    // Smooth coloring
    float zn = sqrt(x*x + y*y);
    float smooth = n + 1 - log(log(zn)) / log(2.0);
    float t = smooth / maxIterations;

    // Gradient: smooth rainbow
    unsigned char r = (unsigned char)(9 * (1 - t) * t * t * t * 255);
    unsigned char g = (unsigned char)(15 * (1 - t) * (1 - t) * t * t * 255);
    unsigned char bCol = (unsigned char)(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255);

    return Color{r, g, bCol, 255};
}



// CODE //

// Tile structure
struct Tile {
  std::mutex texMutex;

  RenderTexture2D texture, oldTexture;
  int tileX, tileY;

  long double a1, b1, a2, b2;
  long double olda1, oldb1, olda2, oldb2;
  int generation = 0;

  Color* pixels;
  bool hasComputed = false;
};

// List of all the tiles
std::vector<Tile> tiles(partsX * partsY);

// Compute a tile in the background
void computeTileThread(int tileIndex, long double cx, long double cy, long double z, int generation) {
  // Get the tile
  Tile& tile = tiles[tileIndex];
  if (tile.hasComputed) return;
  
  // Update generation
  {
    std::lock_guard<std::mutex> lock(tile.texMutex);
    tile.generation = generation;
  }

  // Compute the pixels
  Color* pixels = new Color[partWidth * partHeight];
  for (int y = 0; y < partHeight; y++) {
    for (int x = 0; x < partWidth; x++) {
      long double posx = (x + tile.tileX * partWidth - SCREEN_WIDTH / 2.0) / z + cx;
      long double posy = (y + tile.tileY * partHeight - SCREEN_HEIGHT / 2.0) / z + cy;
      
      if (SET == 0) { pixels[y * partWidth + x] = getColorFromPoint_Mandelbrot(posx, posy, maxIterations); }
      else if (SET == 1) { pixels[y * partWidth + x] = getColorFromPoint_Julia(posx, posy, maxIterations); }
      else if (SET == 2) { pixels[y * partWidth + x] = getColorFromPoint_BurningShip(posx, posy, maxIterations); }
      else if (SET == 3) { pixels[y * partWidth + x] = getColorFromPoint_Tricorn(posx, posy, maxIterations); }
      else if (SET == 4) { pixels[y * partWidth + x] = getColorFromPoint_Phoenix(posx, posy, maxIterations); }
    }
  }
  
  // Lock tile to save computed pixels
  {
    std::lock_guard<std::mutex> lock(tile.texMutex);
    if (tile.generation <= generation) {
      tile.pixels = pixels;
      tile.hasComputed = true;
    }
  }
}

// Launch all tile updates in parallel
void updateTilesParallel(long double cx, long double cy, long double z, int generation) {
  int tileCount = tiles.size();
  
  // Start new detached thread
  if (FAST_MODE) {
    for (int i = 0; i < tileCount; ++i) {
      std::thread([i, cx, cy, z, generation]() {
        computeTileThread(i, cx, cy, z, generation);
      }).detach();
    }
  } else { 
    std::vector<std::thread> workers;
    for (int i = 0; i < tileCount; ++i) { workers.emplace_back(computeTileThread, i, cx, cy, z, generation);  }
    for (auto& t : workers) t.join(); // wait all
  }
}


// Main function
int main() {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Fractal Explorer - Multi-threaded");
  SetTargetFPS(60);

  // Create tile textures
  for (int y = 0; y < partsY; y++) {
    for (int x = 0; x < partsX; x++) {
      Tile& tile = tiles[y * partsX + x];
      tile.tileX = x;
      tile.tileY = y;
      tile.texture = LoadRenderTexture(partWidth, partHeight);
    }
  }

  long double prevCamX = cameraX;
  long double prevCamY = cameraY;
  long double prevZoom = zoom;
  int generation = 2;

  // Make it easier to call the function
  auto customUpdateTilesParallel = [&prevCamX, &prevCamY, &prevZoom](long double cx, long double cy, long double z, int generation) {
    prevCamX = cameraX;
    prevCamY = cameraY;
    prevZoom = zoom;
    updateTilesParallel(cameraX, cameraY, zoom, 0);
  };

  // First render
  customUpdateTilesParallel(cameraX, cameraY, zoom, 0);
  customUpdateTilesParallel(cameraX, cameraY, zoom, 1);

  // Main loop
  while (!WindowShouldClose()) {
    // Camera movement and zoom
    if (IsKeyDown(KEY_W)) { cameraY -= cameraSpeed / zoom / 60; }
    if (IsKeyDown(KEY_S)) { cameraY += cameraSpeed / zoom / 60; }
    if (IsKeyDown(KEY_A)) { cameraX -= cameraSpeed / zoom / 60; }
    if (IsKeyDown(KEY_D)) { cameraX += cameraSpeed / zoom / 60; }
    if (IsKeyDown(KEY_UP)) { zoom += zoomSpeed * zoom / 60; }
    if (IsKeyDown(KEY_DOWN)) { zoom -= zoomSpeed * zoom / 60; }
    // Change number of iterations
    if (IsKeyPressed(KEY_LEFT)) {
      maxIterations -= 100;
      generation++;
      customUpdateTilesParallel(cameraX, cameraY, zoom, generation);
    }
    if (IsKeyPressed(KEY_RIGHT)) {
      maxIterations += 100;
      generation++;
      customUpdateTilesParallel(cameraX, cameraY, zoom, generation);
    }
    if (IsKeyPressed(KEY_R)) { // Reset view
      cameraX = 0;
      cameraY = 0;
      zoom = SCREEN_WIDTH / 3;
      generation++;
      customUpdateTilesParallel(cameraX, cameraY, zoom, generation);
    }

    // Automatically re-render the fractal if the view moved too much
    float acceptedChange = 0.1f / zoom * 1000 * cameraAceptedChange;
    if (IsKeyPressed(KEY_SPACE) || abs(cameraX - prevCamX) >= acceptedChange || abs(cameraY - prevCamY) >= acceptedChange || abs(1 - (zoom / prevZoom)) >= zoomAceptedChange) {
      generation++;
      customUpdateTilesParallel(cameraX, cameraY, zoom, generation);
    }

    // Iterate trough each tile to check if needed to copy pixels to texture
    for (auto& tile : tiles) {
      if (!tile.hasComputed) continue;
      {
        std::lock_guard<std::mutex> lock(tile.texMutex);

        // To not get visual glitches
        if (USE_OLD_TEXTURES) {
          Image tempImage = LoadImageFromTexture(tile.texture.texture);
          UnloadTexture(tile.oldTexture.texture);
          tile.oldTexture.texture = LoadTextureFromImage(tempImage);
          UnloadImage(tempImage);
          tile.olda1 = tile.a1;
          tile.oldb1 = tile.b1;
          tile.olda2 = tile.a2;
          tile.oldb2 = tile.b2;
        }
        
        // Save the old camera position from when it was computed
        UpdateTexture(tile.texture.texture, tile.pixels);
        tile.a1 = (tile.tileX * partWidth - SCREEN_WIDTH / 2.0) / prevZoom + prevCamX;
        tile.b1 = (tile.tileY * partHeight - SCREEN_HEIGHT / 2.0) / prevZoom + prevCamY;
        tile.a2 = ((tile.tileX + 1) * partWidth - SCREEN_WIDTH / 2.0) / prevZoom + prevCamX;
        tile.b2 = ((tile.tileY + 1) * partHeight - SCREEN_HEIGHT / 2.0) / prevZoom + prevCamY;

        delete[] tile.pixels;
        tile.hasComputed = false;
      }
    }

    // Drawing
    BeginDrawing();
      ClearBackground(BLACK);

      // Draw the old textures to stop visual glitches
      if (USE_OLD_TEXTURES) {
        for (auto& tile : tiles) {
          {
            std::lock_guard<std::mutex> lock(tile.texMutex);

            // Calculate the right position to show the old pixels, based on where they were computed
            float startX = (tile.olda1 - cameraX) * zoom + SCREEN_WIDTH / 2.0;
            float startY = (tile.oldb1 - cameraY) * zoom + SCREEN_HEIGHT / 2.0;
            float endX = (tile.olda2 - cameraX) * zoom + SCREEN_WIDTH / 2.0;
            float endY = (tile.oldb2 - cameraY) * zoom + SCREEN_HEIGHT / 2.0;
            
            DrawTexturePro(tile.oldTexture.texture,
              { 0, 0, (float) partWidth, (float) partHeight },
              { startX, startY, endX - startX, endY - startY },
              { 0, 0 }, 0, WHITE);
            
            // Only for debug
            if (SHOW_PARTS) { DrawRectangleLines(startX, startY, endX - startX, endY - startY, RED); }
          }
        }
      }

      // Draw all tiles
      for (auto& tile : tiles) {
        {
          std::lock_guard<std::mutex> lock(tile.texMutex);

          // Calculate the right position to show the old pixels, based on where they were computed
          float startX = (tile.a1 - cameraX) * zoom + SCREEN_WIDTH / 2.0;
          float startY = (tile.b1 - cameraY) * zoom + SCREEN_HEIGHT / 2.0;
          float endX = (tile.a2 - cameraX) * zoom + SCREEN_WIDTH / 2.0;
          float endY = (tile.b2 - cameraY) * zoom + SCREEN_HEIGHT / 2.0;
          
          DrawTexturePro(tile.texture.texture,
            { 0, 0, (float) partWidth, (float) partHeight },
            { startX, startY, endX - startX, endY - startY },
            { 0, 0 }, 0, WHITE);
          
          // Only for debug
          if (SHOW_PARTS) { DrawRectangleLines(startX, startY, endX - startX, endY - startY, BLUE); }
        }
      }

      // Draw UI
      DrawText(TextFormat("Iterations: %.0f", maxIterations), 10, 10, 20, WHITE);
      DrawText(TextFormat("Generation: %.0f", (float) generation), 10, 30, 20, WHITE);
      DrawText(TextFormat("Threads: %.0f", (float) partsX * partsY), 10, 50, 20, WHITE);
      DrawText(TextFormat("Zoom: %.2f | %.2f", zoom, zoom / prevZoom), 10, 70, 20, WHITE);
    EndDrawing();
  }

  // Unload all textures from memory
  for (auto& tile : tiles) {
    UnloadRenderTexture(tile.texture);
  }

  CloseWindow();
  return 0;
}
