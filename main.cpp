#include "raylib.h"
#include <thread>


// Constants
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const int partsX = 16;
const int partsY = 9;
const bool showParts = false;
const int partWidth = SCREEN_WIDTH / partsX;
const int partHeight = SCREEN_HEIGHT / partsY;

// Camera
long double cameraX = 0;
long double cameraY = 0;
const float cameraSpeed = 500.0f;
long double zoom = 500;
const float zoomSpeed = 1.5f;
float maxIterations = 100;

// Julia Set
const long double julia_ca = -0.7;    // Real part of c
const long double julia_cb = 0.27015; // Imaginary part of c


// Tile structure
struct Tile {
  std::mutex texMutex;

  RenderTexture2D texture;
  int tileX, tileY;

  Color* pixels;
  bool ready = false;
  bool hasComputed = false;
};

// List of all the tiles
std::vector<Tile> tiles(partsX * partsY);

// Fractal Coloring Function
const long double pisqrtpi = PI * sqrt(PI);
Color getColorFromPoint(long double a, long double b, float maxIterations) {
  long double ca = a;
  long double cb = b;
  int n;
  long double aa, bb;
  for (n = 0; (abs(a + b) <= 16) && (n < maxIterations); n++) {
    aa = a * a - b * b + ca;
    b = 2.0 * a * b + cb;
    a = aa;
  }
  Color color = BLACK;
  if (n < maxIterations) {
    color.a = 255;
    color.r = ((int)(n * PI)) % 255;
    color.g = n % 255;
    color.b = ((int)(n * pisqrtpi)) % 255;
  }
  return color;
}

// Julia Set
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

Color getColorFromPoint_JuliaSet(long double a, long double b, float maxIterations) {
  const long double ca = -0.7;
  const long double cb = 0.27015;

  int n = 0;
  long double aa, bb;

  for (; n < maxIterations; ++n) {
    if ((a*a + b*b) > 4.0) break;
    aa = a * a - b * b + ca;
    bb = 2.0 * a * b + cb;
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

// Compute a tile in the background
void computeTileThread(int tileIndex, long double cx, long double cy, long double z) {
  // Get the tile
  Tile& tile = tiles[tileIndex];
  if (tile.hasComputed) return;

  // Compute the pixels
  Color* pixels = new Color[partWidth * partHeight];
  for (int y = 0; y < partHeight; y++) {
    for (int x = 0; x < partWidth; x++) {
      long double posx = (x + tile.tileX * partWidth - SCREEN_WIDTH / 2.0) / z + cx;
      long double posy = (y + tile.tileY * partHeight - SCREEN_HEIGHT / 2.0) / z + cy;
      pixels[y * partWidth + x] = getColorFromPoint(posx, posy, maxIterations);
    }
  }
  
  // Lock tile to save computed pixels
  {
    std::lock_guard<std::mutex> lock(tile.texMutex);
    tile.pixels = pixels;
    tile.hasComputed = true;
    tile.ready = false;
  }
}

// Launch all tile updates in parallel
void updateTilesParallel(long double cx, long double cy, long double z) {
  std::vector<std::thread> workers;
  int tileCount = tiles.size();
  for (int i = 0; i < tileCount; ++i) { workers.emplace_back(computeTileThread, i, cx, cy, z);  }
  for (auto& t : workers) t.join(); // wait all
}


// Main function
int main() {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Mandelbrot Fractal - Multi-threaded");
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

  // First render
  updateTilesParallel(cameraX, cameraY, zoom);

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
      updateTilesParallel(cameraX, cameraY, zoom);
    }
    if (IsKeyPressed(KEY_RIGHT)) {
      maxIterations += 100;
      updateTilesParallel(cameraX, cameraY, zoom);
    }
    if (IsKeyPressed(KEY_R)) { // Reset view
      cameraX = 0;
      cameraY = 0;
      zoom = SCREEN_WIDTH / 3;
      updateTilesParallel(cameraX, cameraY, zoom);
    }
    if (cameraX != prevCamX || cameraY != prevCamY || zoom != prevZoom) { // Detect camera change
      prevCamX = cameraX;
      prevCamY = cameraY;
      prevZoom = zoom;
      updateTilesParallel(cameraX, cameraY, zoom);
    }

    // Iterate trough each tile to check if needed to copy pixels to texture
    for (auto& tile : tiles) {
      if (!tile.hasComputed || tile.ready) continue;
      {
        std::lock_guard<std::mutex> lock(tile.texMutex);
        UpdateTexture(tile.texture.texture, tile.pixels);
        delete[] tile.pixels;
        tile.ready = true;
        tile.hasComputed = false;
      }
    }

    BeginDrawing();
      ClearBackground(BLACK);

      // Draw all tiles
      for (auto& tile : tiles) {
        if (tile.ready) {
          {
            std::lock_guard<std::mutex> lock(tile.texMutex);
            DrawTextureRec(tile.texture.texture,
              { 0, 0, (float) partWidth, (float) partHeight },
              { (float) (tile.tileX * partWidth), (float) (tile.tileY * partHeight) },
            WHITE);
            
            // Only for debug
            if (showParts) { DrawRectangleLines(tile.tileX * partWidth, tile.tileY * partHeight, partWidth, partHeight, RED); }
          }
        }
      }

      // Draw UI
      DrawText(TextFormat("Iterations: %.0f", maxIterations), 10, 10, 20, WHITE);
      DrawText(TextFormat("Threads: %.0f", (float) partsX * partsY), 10, 30, 20, WHITE);
      DrawText(TextFormat("Zoom: %.2f", zoom), 10, 50, 20, WHITE);
    EndDrawing();
  }

  // Unload all textures from memory
  for (auto& tile : tiles) {
    UnloadRenderTexture(tile.texture);
  }

  CloseWindow();
  return 0;
}
