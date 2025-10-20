#include <unordered_set>
#include <thread>
#include <iostream>

// Own implementations
#include "sets_definition.hpp"


// Constants (changeable with flags)
bool FULLSCREEN = false;
int SCREEN_WIDTH = 1600;
int SCREEN_HEIGHT = 900;
// What set to display | 0 : Mandelbrot | 1 : Julia | 2 : Burning ship | 3 : Tricorn | 4 : Phoenix | 5 : Lyapunov | 6 : Mandelbrot Light Effect
int SET = 0;
int MAX_ITERATIONS = 2000;
int TARGET_FPS = 90;

// How many horizontal and vertical tiles to create
const int TILES_X = 16;
const int TILES_Y = 9;
// Debug tool to visualize the individual tiles (can toggle with LSHIFT)
bool SHOW_TILES = false;

// Detaches the threads, makes the app smoother but can cause a lot of visual glitches at high iterations and big zoom
// If set to false, there are no visual glitches but the app is a lot slower and has freezes at high iterations
bool DETACHED_MODE = true;
const int MAX_THREADS = std::thread::hardware_concurrency();
// Should be set to true, avoids unnecessary re-renders of the same tile, makes the app faster but transitions can be worse
bool AVOID_DUPLICATES = true;
// Should reduce black frames, but slows down the app (can introduce some stutters)
bool USE_OLD_TEXTURES = true;

// What change in zoom should trigger a re-render of the view (0.5 -> 50%)
const float zoomAcceptedChange = 0.25f;
// What change in position should trigger a re-render of the view
const float cameraAcceptedChange = 0.25f * 1000.0f;

// Initial position and settings of the camera
long double cameraX = 0;
long double cameraY = 0;
float cameraSpeed = 500.0f;

long double zoom = 500;
float zoomSpeed = 0.85f;

float TILE_WIDTH, TILE_HEIGHT;
float HALF_SCREEN_WIDTH, HALF_SCREEN_HEIGHT;

// Multi-threading
std::atomic<int> runningThreads(0);
struct PendingTile {
  int index;
  long double cx, cy, cz;
  int generation;
  float maxIterations;
};
std::deque<PendingTile> pendingTiles;
std::unordered_set<int> tilesScheduled; // To avoid duplicates in queue

// CODE //

// Tile structure
struct Tile {
  // Lock used to modify the tile in different threads
  std::mutex texMutex;

  // Textures
  RenderTexture2D texture, oldTexture, veryOldTexture;
  int tileX, tileY;

  // Coordinates of the top left corner of the tile, and zoom from when it was scheduled
  long double x, y, z;
  // Coordinates of the camera when the tile was computed
  long double cx, cy, cz;

  // Old positions to draw the old texture
  long double oldX, oldY, oldZ;
  long double veryOldX, veryOldY, veryOldZ;
  
  // Actual pixel information of the tile
  Color *pixels;
  bool hasComputed = false;
  int generation = 0;
};

// List of all the tiles
std::vector<Tile> tiles(TILES_X *TILES_Y);

// Compute a tile in the background
void computeTileThread(int tileIndex, long double cx, long double cy, long double cz, int generation, float maxIterations) {
  // Add one to the thread counter
  runningThreads.fetch_add(1, std::memory_order_relaxed);

  // Get the tile
  Tile &tile = tiles[tileIndex];

  // Update generation count
  {
    std::lock_guard<std::mutex> lock(tile.texMutex);
    if (tile.generation <= generation) {
      tile.generation = generation;
    }
  }

  // Compute the pixels
  int jTileWidth;
  long double x, y;
  Color *pixels = new Color[TILE_WIDTH * TILE_HEIGHT];
  for (int j = 0; j < TILE_HEIGHT; j++) {
    jTileWidth = j * TILE_WIDTH;
    for (int i = 0; i < TILE_WIDTH; i++) {
      x = (i + tile.tileX * TILE_WIDTH - HALF_SCREEN_WIDTH) / cz + cx;
      y = (j + tile.tileY * TILE_HEIGHT - HALF_SCREEN_HEIGHT) / cz + cy;

      switch (SET) {
        case 0: pixels[jTileWidth + i] = getColorFromPoint_Mandelbrot(x, y, maxIterations);  break;
        case 1: pixels[jTileWidth + i] = getColorFromPoint_Julia(x, y, maxIterations);  break;
        case 2: pixels[jTileWidth + i] = getColorFromPoint_BurningShip(x, y, maxIterations);  break;
        case 3: pixels[jTileWidth + i] = getColorFromPoint_Tricorn(x, y, maxIterations);  break;
        case 4: pixels[jTileWidth + i] = getColorFromPoint_Phoenix(x, y, maxIterations);  break;
        case 5: pixels[jTileWidth + i] = getColorFromPoint_Lyapunov(x, y, maxIterations);  break;
        case 6: pixels[jTileWidth + i] = getColorFromPoint_Mandelbrot_LightEffect(x, y, maxIterations);  break;
        
        default: break;
      }
    }
  }

  // Lock tile to save computed pixels
  {
    std::lock_guard<std::mutex> lock(tile.texMutex);
    if (tile.generation <= generation) {
      tile.generation = generation;
      tile.pixels = pixels;
      tile.hasComputed = true;
      tile.cx = cx;
      tile.cy = cy;
      tile.cz = cz;
    }
  }

  // Remove one from the thread counter
  runningThreads.fetch_sub(1, std::memory_order_relaxed);
}

// Do a spiral
std::vector<int> getSpiralIndicesOutward(int TILES_X, int TILES_Y) {
  std::vector<int> result;
  std::vector<std::vector<bool>> visited(TILES_Y, std::vector<bool>(TILES_X, false));

  // Starting position (center)
  int startX = TILES_X / 2;
  int startY = TILES_Y / 2;

  // Direction vectors: right, down, left, up
  int dx[] = {1, 0, -1, 0};
  int dy[] = {0, 1, 0, -1};
  int direction = 0;

  int x = startX, y = startY;
  int steps = 1;

  // Add center point
  result.push_back(x + y * TILES_X);
  visited[y][x] = true;

  while (result.size() < TILES_X * TILES_Y) {
    for (int i = 0; i < 2; i++) { // Move in current direction twice per spiral layer
      for (int step = 0; step < steps; step++) {
        x += dx[direction];
        y += dy[direction];

        if (x >= 0 && x < TILES_X && y >= 0 && y < TILES_Y && !visited[y][x]) {
          result.push_back(x + y * TILES_X);
          visited[y][x] = true;
        }
      }
      direction = (direction + 1) % 4; // Turn 90 degrees
    }
    steps++; // Increase step count for next spiral layer
  }

  return result;
}

// Launch all tile updates in parallel
std::vector<int> spiralIndicesOutward = getSpiralIndicesOutward(TILES_X, TILES_Y);
void updateTilesParallel(long double cx, long double cy, long double cz, int generation, float maxIterations, long double diffX, long double diffY) {
  int tileCount = tiles.size();

  if (DETACHED_MODE) {
    // Adds the tile to the queue, with all needed information to compute the pixels
    auto scheduleTile = [cx, cy, cz, generation, maxIterations] (int i) {
      PendingTile pendingTile = {i, cx, cy, cz, generation, maxIterations};

      // Remove tile from pending list if it was already scheduled (optional, but makes the app faster)
      if (AVOID_DUPLICATES && tilesScheduled.find(i) != tilesScheduled.end()) {
        for (auto it = pendingTiles.begin(); it != pendingTiles.end(); ++it) {
          if (it->index == i) {
            pendingTiles.erase(it);
            break;
          }
        }
      }

      // Add tile to the queue
      pendingTiles.push_back(pendingTile);
      tilesScheduled.insert(i);
    };

    // Do a spiral pattern if simply zooming in or out
    if (diffX == 0 && diffY == 0) {
      for (const int index : spiralIndicesOutward) {
        scheduleTile(index);
      }
      return;
    }

    // Change the order of the tiles based on movement direction
    if (diffX >= 0) {
      if (diffY >= 0) {
        // Top left
        for (int i = 0; i < TILES_X; ++i) {
          for (int j = 0; j < TILES_Y; ++j) {
            scheduleTile(j * TILES_X + i);
          }
        }
      }
      else {
        // Bottom left
        for (int i = 0; i < TILES_X; ++i) {
          for (int j = TILES_Y - 1; j >= 0; --j) {
            scheduleTile(j * TILES_X + i);
          }
        }
      }
    }
    else {
      if (diffY >= 0) {
        // Top right
        for (int i = TILES_X - 1; i >= 0; --i) {
          for (int j = 0; j < TILES_Y; ++j) {
            scheduleTile(j * TILES_X + i);
          }
        }
      }
      else {
        // Bottom right
        for (int i = TILES_X - 1; i >= 0; --i) {
          for (int j = TILES_Y - 1; j >= 0; --j) {
            scheduleTile(j * TILES_X + i);
          }
        }
      }
    }
  }
  else {
    // Compute all threads at the same time
    std::vector<std::thread> workers;
    for (int i = 0; i < tileCount; ++i) {
      workers.emplace_back(computeTileThread, i, cx, cy, cz, generation, maxIterations);
    }
    for (auto &t : workers) {
      t.join(); // waits for all threads
    }
  }
}

// Main function
int main(int argc, char* argv[]) {
  // Replace constants by the ones given in the flags (if present)
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--fullscreen") {
      FULLSCREEN = true;
    } else if (arg == "--width") {
      SCREEN_WIDTH = std::stoi(argv[++i]);
    } else if (arg == "--height") {
      SCREEN_HEIGHT = std::stoi(argv[++i]);
    } else if (arg == "--set") {
      SET = std::stoi(argv[++i]);
    } else if (arg == "--it") {
      MAX_ITERATIONS = std::stoi(argv[++i]);
    } else if (arg == "--fps") {
      TARGET_FPS = std::stoi(argv[++i]);
    } else if (arg == "--show-tiles") {
      SHOW_TILES = true;
    } else if (arg == "--no-detached") {
      DETACHED_MODE = false;
    } else if (arg == "--no-avoid-duplicates") {
      AVOID_DUPLICATES = false;
    } else if (arg == "--no-old-textures") {
      USE_OLD_TEXTURES = false;
    } else if (arg == "--zoom") {
      zoom = std::stold(argv[++i]);
    } else if (arg == "--x") {
      cameraX = std::stold(argv[++i]);
    } else if (arg == "--y") {
      cameraY = std::stold(argv[++i]);
    } else if (arg == "--speed") {
      cameraSpeed = std::stof(argv[++i]);
    } else if (arg == "--zoom-speed") {
      zoomSpeed = std::stof(argv[++i]);
    }
  }
  
  if (FULLSCREEN) {
    SetConfigFlags(FLAG_WINDOW_TOPMOST | FLAG_WINDOW_UNDECORATED);
    InitWindow(0, 0, "Fractal Explorer - Multi-threaded");
    HideCursor();
    SCREEN_WIDTH = GetScreenWidth();
    SCREEN_HEIGHT = GetScreenHeight() - 35;
    TILE_WIDTH = SCREEN_WIDTH / TILES_X;
    TILE_HEIGHT = SCREEN_HEIGHT / TILES_Y;
  }
  else {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Fractal Explorer - Multi-threaded");
  }
  SetTargetFPS(TARGET_FPS);

  // Compute values based of the given flags
  TILE_WIDTH = SCREEN_WIDTH / TILES_X;
  TILE_HEIGHT = SCREEN_HEIGHT / TILES_Y;
  HALF_SCREEN_WIDTH = SCREEN_WIDTH / 2.0;
  HALF_SCREEN_HEIGHT = SCREEN_HEIGHT / 2.0;
  const float cameraMovementPerFrame = cameraSpeed / TARGET_FPS;
  const float zoomPerFrame = zoomSpeed / TARGET_FPS;

  // Create tile textures
  for (int y = 0; y < TILES_Y; y++) {
    for (int x = 0; x < TILES_X; x++) {
      Tile &tile = tiles[y * TILES_X + x];
      tile.tileX = x;
      tile.tileY = y;
      tile.texture = LoadRenderTexture(TILE_WIDTH, TILE_HEIGHT);
      tile.oldTexture = LoadRenderTexture(TILE_WIDTH, TILE_HEIGHT);
      tile.veryOldTexture = LoadRenderTexture(TILE_WIDTH, TILE_HEIGHT);
    }
  }

  // Needed to detect camera and zoom change
  long double prevCamX = cameraX;
  long double prevCamY = cameraY;
  long double prevZoom = zoom;
  int generation = 0;
  float maxIterations = MAX_ITERATIONS;
  bool showPointer = false;

  // Make it easier to call the function
  auto customUpdateTilesParallel = [&prevCamX, &prevCamY, &prevZoom, &maxIterations, &generation]() {
    updateTilesParallel(cameraX, cameraY, zoom, generation, maxIterations, prevCamX - cameraX, prevCamY - cameraY);
    prevCamX = cameraX;
    prevCamY = cameraY;
    prevZoom = zoom;
    generation++;
  };

  // First render
  customUpdateTilesParallel();

  // Main loop
  while (!WindowShouldClose()) {
    // Camera movement and zoom
    if (IsKeyDown(KEY_W)) { cameraY -= cameraMovementPerFrame / zoom; }
    if (IsKeyDown(KEY_S)) { cameraY += cameraMovementPerFrame / zoom; }
    if (IsKeyDown(KEY_A)) { cameraX -= cameraMovementPerFrame / zoom; }
    if (IsKeyDown(KEY_D)) { cameraX += cameraMovementPerFrame / zoom; }
    if (IsKeyDown(KEY_UP)) { zoom *= (1 + zoomPerFrame); }
    if (IsKeyDown(KEY_DOWN)) { zoom *= (1 - zoomPerFrame); }
    if (IsKeyPressed(KEY_V)) { showPointer = !showPointer; }
    // Debug tools
    if (IsKeyPressed(KEY_LEFT_SHIFT)) { SHOW_TILES = !SHOW_TILES; }
    if (IsKeyPressed(KEY_O)) { // Change set (-1)
      SET = (SET - 1) % 7;
      if (SET < 0) { SET = 7 + SET; }
      customUpdateTilesParallel();
    }
    if (IsKeyPressed(KEY_P)) { // Change set (+1)
      SET = (SET + 1) % 7;
      customUpdateTilesParallel();
    }
    if (IsKeyPressed(KEY_LEFT)) { // Change number of iterations
      maxIterations -= 100;
      customUpdateTilesParallel();
    }
    if (IsKeyPressed(KEY_RIGHT)) {
      maxIterations += 100;
      customUpdateTilesParallel();
    }
    if (IsKeyPressed(KEY_R)) { // Reset view
      cameraX = 0;
      cameraY = 0;
      zoom = SCREEN_WIDTH / 3;
      customUpdateTilesParallel();
    }
    if (IsKeyPressed(KEY_C)) { // Output camera position and zoom
      std::cout << TextFormat("Zoom: %.36f", (float) zoom) << std::endl;
      std::cout << TextFormat("Camera X: %.36f", (float) cameraX) << std::endl;
      std::cout << TextFormat("Camera Y: %.36f", (float) cameraY) << std::endl;
    }

    // Automatically re-render the fractal if the view moved too much
    float acceptedChange = cameraAcceptedChange / zoom;
    if (IsKeyPressed(KEY_SPACE) || abs(cameraX - prevCamX) >= acceptedChange || abs(cameraY - prevCamY) >= acceptedChange || abs(1 - (zoom / prevZoom)) >= zoomAcceptedChange) {
      customUpdateTilesParallel();
    }

    // Start to render pending tiles
    while (runningThreads.load(std::memory_order_relaxed) < MAX_THREADS && !pendingTiles.empty()) {
      PendingTile next = pendingTiles.front();
      pendingTiles.pop_front();
      tilesScheduled.erase(next.index);

      // Check that the tile has not already been computed by a newer generation
      Tile &tile = tiles[next.index];
      if (next.generation - tile.generation >= 0) {
        std::thread(computeTileThread, next.index, next.cx, next.cy, next.cz, next.generation, next.maxIterations).detach();
      }
    }

    // Iterate trough each tile to check if needed to copy pixels to texture
    for (auto &tile : tiles) {
      if (!tile.hasComputed) { continue; }
      {
        std::lock_guard<std::mutex> lock(tile.texMutex);

        // To not get visual glitches
        if (USE_OLD_TEXTURES) {
          // Draw oldTexture on veryOldTexture
          BeginTextureMode(tile.veryOldTexture);
          DrawTexturePro(tile.oldTexture.texture,
                         {0, TILE_HEIGHT, TILE_WIDTH, -TILE_HEIGHT},
                         {0, 0, TILE_WIDTH, TILE_HEIGHT},
                         {0, 0}, 0, WHITE);
          EndTextureMode();
          tile.veryOldX = tile.oldX;
          tile.veryOldY = tile.oldY;
          tile.veryOldZ = tile.oldZ;

          // Draw texture on oldTexture
          BeginTextureMode(tile.oldTexture);
          DrawTexturePro(tile.texture.texture,
                         {0, TILE_HEIGHT, TILE_WIDTH, -TILE_HEIGHT},
                         {0, 0, TILE_WIDTH, TILE_HEIGHT},
                         {0, 0}, 0, WHITE);
          EndTextureMode();
          tile.oldX = tile.x;
          tile.oldY = tile.y;
          tile.oldZ = tile.z;
        }

        // Save the old camera position from when it was computed
        UpdateTexture(tile.texture.texture, tile.pixels);
        tile.x = (tile.tileX * TILE_WIDTH - HALF_SCREEN_WIDTH) / tile.cz + tile.cx;
        tile.y = (tile.tileY * TILE_HEIGHT - HALF_SCREEN_HEIGHT) / tile.cz + tile.cy;
        tile.z = tile.cz;

        delete[] tile.pixels;
        tile.hasComputed = false;
      }
    }

    // Actual drawing
    BeginDrawing();
    ClearBackground(BLACK);

    // Draw the old textures to stop visual glitches
    if (USE_OLD_TEXTURES) {
      // Very old texture
      for (auto &tile : tiles) {
        {
          std::lock_guard<std::mutex> lock(tile.texMutex);

          // Calculate the right position to show the old pixels, based on where they were computed
          float x = (tile.veryOldX - cameraX) * zoom + HALF_SCREEN_WIDTH;
          float y = (tile.veryOldY - cameraY) * zoom + HALF_SCREEN_HEIGHT;
          float w = TILE_WIDTH / tile.veryOldZ * zoom;
          float h = TILE_HEIGHT / tile.veryOldZ * zoom;

          DrawTexturePro(tile.veryOldTexture.texture,
                         {0, 0, TILE_WIDTH, TILE_HEIGHT},
                         {x, y, w, h},
                         {0, 0}, 0, WHITE);

          // Only for debug
          if (SHOW_TILES) {
            DrawRectangleLines(x, y, w, h, GREEN);
          }
        }
      }

      // Old texture
      for (auto &tile : tiles) {
        {
          std::lock_guard<std::mutex> lock(tile.texMutex);

          // Calculate the right position to show the old pixels, based on where they were computed
          float x = (tile.oldX - cameraX) * zoom +  HALF_SCREEN_WIDTH;
          float y = (tile.oldY - cameraY) * zoom + HALF_SCREEN_HEIGHT;
          float w = TILE_WIDTH / tile.oldZ * zoom;
          float h = TILE_HEIGHT / tile.oldZ * zoom;

          DrawTexturePro(tile.oldTexture.texture,
                         {0, 0, TILE_WIDTH, TILE_HEIGHT},
                         {x, y, w, h},
                         {0, 0}, 0, WHITE);

          // Only for debug
          if (SHOW_TILES) {
            DrawRectangleLines(x, y, w, h, RED);
          }
        }
      }
    }

    // Draw all tiles
    for (auto &tile : tiles) {
      {
        std::lock_guard<std::mutex> lock(tile.texMutex);

        // Calculate the right position to show the pixels, based on where they were computed
        float x = (tile.x - cameraX) * zoom + HALF_SCREEN_WIDTH;
        float y = (tile.y - cameraY) * zoom + HALF_SCREEN_HEIGHT;
        float w = TILE_WIDTH / tile.z * zoom;
        float h = TILE_HEIGHT / tile.z * zoom;

        DrawTexturePro(tile.texture.texture,
                       {0, 0, TILE_WIDTH, TILE_HEIGHT},
                       {x, y, w, h},
                       {0, 0}, 0, WHITE);

        // Only for debug
        if (SHOW_TILES) {
          DrawRectangleLines(x, y, w, h, BLUE);
        }
      }
    }

    // Draw UI
    DrawText(TextFormat("Iterations: %.0f", maxIterations), 10, 10, 20, WHITE);
    DrawText(TextFormat("Generation: %.0f", (float) generation), 10, 30, 20, WHITE);
    DrawText(TextFormat("Tiles: %.0f", (float) (TILES_X * TILES_Y)), 10, 50, 20, WHITE);

    DrawText(TextFormat("Threads: %.0f", (float) runningThreads.load(std::memory_order_relaxed)), SCREEN_WIDTH - 10 - MeasureText(TextFormat("Threads: %.0f", (float) runningThreads.load(std::memory_order_relaxed)), 20), 10, 20, WHITE);
    DrawText(TextFormat("Queue: %.0f", (float) pendingTiles.size()), SCREEN_WIDTH - 10 - MeasureText(TextFormat("Queue: %.0f", (float) pendingTiles.size()), 20), 30, 20, WHITE);
    DrawText(TextFormat("FPS: %.0f", (float) GetFPS()), SCREEN_WIDTH - 10 - MeasureText(TextFormat("FPS: %.0f", (float) GetFPS()), 20), 50, 20, WHITE);

    DrawText(TextFormat("Camera X: %.15f", (float) cameraX), 10, SCREEN_HEIGHT - 70, 20, WHITE);
    DrawText(TextFormat("Camera Y: %.15f", (float) cameraY), 10, SCREEN_HEIGHT - 50, 20, WHITE);
    DrawText(TextFormat("Zoom: %.2f | %.2f", zoom, zoom / prevZoom), 10, SCREEN_HEIGHT - 30, 20, WHITE);

    DrawText(TextFormat("%.0f x %.0f", (float) SCREEN_WIDTH, (float) SCREEN_HEIGHT), SCREEN_WIDTH - 10 - MeasureText(TextFormat("%.0f x %.0f", (float) SCREEN_WIDTH, (float) SCREEN_HEIGHT), 20), SCREEN_HEIGHT - 30, 20, WHITE);
    
    if (showPointer) {
      DrawLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 5, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 5, WHITE);
      DrawLine(SCREEN_WIDTH / 2 - 5, SCREEN_HEIGHT / 2, SCREEN_WIDTH / 2 + 5, SCREEN_HEIGHT / 2, WHITE);
    }
    EndDrawing();
  }

  // Unload all textures from memory
  for (auto &tile : tiles) {
    UnloadRenderTexture(tile.texture);
    UnloadRenderTexture(tile.oldTexture);
    UnloadRenderTexture(tile.veryOldTexture);
  }

  CloseWindow();

  // To then copy and paste if needed
  std::cout << "## Final view ##" << std::endl;
  std::cout << TextFormat("Zoom: %.36f", (float) zoom) << std::endl;
  std::cout << TextFormat("Camera X: %.36f", (float) cameraX) << std::endl;
  std::cout << TextFormat("Camera Y: %.36f", (float) cameraY) << std::endl;

  return 0;
}
