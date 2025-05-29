#include "raylib.h"
#include <vector>
#include <thread>
#include <cmath>
#include <mutex>
#include <queue>
#include <iostream>

// Constants
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const int partsX = 16;
const int partsY = 9;
const int partWidth = SCREEN_WIDTH / partsX;
const int partHeight = SCREEN_HEIGHT / partsY;
const float cameraSpeed = 500.0f;
const float zoomSpeed = 1.5f;
float maxIterations = 100;

// Camera
long double cameraX = 0;
long double cameraY = 0;
long double zoom = SCREEN_WIDTH / 3;


// Tile structure
struct Tile {
  RenderTexture2D texture;
  int tileX, tileY;
  bool ready = false;
  std::mutex texMutex;
};
// List of all the tiles
std::vector<Tile> tiles(partsX * partsY);

// Used to compute in parallel
struct TileUpdate {
  int tileIndex;
  Color* pixels;
};
std::mutex queueMutex;
std::condition_variable queueCond;
std::queue<TileUpdate> uploadQueue;

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

// Compute a tile in the background
void computeTileThread(int tileIndex, float cx, float cy, float z) {
  Tile& tile = tiles[tileIndex];
  Color* pixels = new Color[partWidth * partHeight];

  for (int y = 0; y < partHeight; y++) {
    for (int x = 0; x < partWidth; x++) {
      long double posx = (x + tile.tileX * partWidth - SCREEN_WIDTH / 2.0) / z + cx;
      long double posy = (y + tile.tileY * partHeight - SCREEN_HEIGHT / 2.0) / z + cy;
      pixels[y * partWidth + x] = getColorFromPoint(posx, posy, maxIterations);
    }
  }

  // Push to upload queue
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    uploadQueue.push({tileIndex, pixels});
  }
  queueCond.notify_one();
}

// Launch all tile updates in parallel
void updateTilesParallel(float cx, float cy, float z) {
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

    // Reset view
    if (IsKeyPressed(KEY_R)) {
      cameraX = 0;
      cameraY = 0;
      zoom = SCREEN_WIDTH / 3;
    }

    // Detect camera change
    if (cameraX != prevCamX || cameraY != prevCamY || zoom != prevZoom) {
      prevCamX = cameraX;
      prevCamY = cameraY;
      prevZoom = zoom;
      updateTilesParallel(cameraX, cameraY, zoom);
    }

    // Update the tiles in parallel
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      while (!uploadQueue.empty()) {
        TileUpdate update = uploadQueue.front();
        uploadQueue.pop();
        Tile& tile = tiles[update.tileIndex];
        UpdateTexture(tile.texture.texture, update.pixels);
        tile.ready = true;
        delete[] update.pixels;
      }
    }


    BeginDrawing();
      ClearBackground(BLACK);

      // Draw all tiles
      for (auto& tile : tiles) {
        if (tile.ready) {
          std::lock_guard<std::mutex> lock(tile.texMutex);
          DrawTextureRec(tile.texture.texture,
            { 0, 0, (float) partWidth, (float) partHeight },
            { (float) (tile.tileX * partWidth), (float) (tile.tileY * partHeight) },
          WHITE);
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
