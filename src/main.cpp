#include "raylib.h"
#include <thread>
#include <unordered_set>
#include <iostream>

// Constants
const bool FULLSCREEN = true;
int SCREEN_WIDTH = 1600;
int SCREEN_HEIGHT = 900;
// What set to display | 0 : Mandebrot | 1 : Julia | 2 : Burning ship | 3 : Tricorn | 4 : Phoenix
const int SET = 0;
const int MAX_ITERATIONS = 2000;
const int TARGET_FPS = 120;

// How many horizontal and vertical tiles to create
const int partsX = 16;
const int partsY = 9;
// Debug tool to visualize the individual tiles (can toggle with LSHIFT)
bool SHOW_PARTS = false;

// Detaches the threads, makes the app smoother but can cause a lot of visual glitches at high iterations and big zoom
// If set to false, there are no visual glitches but the app is a lot slower and has freezes at high iterations
const bool DETACHED_MODE = true;
const int MAX_THREADS = std::thread::hardware_concurrency();
// Should be set to true, avoids unnecessary re-renders of the same tile, makes the app faster but transitions can be worse
const bool AVOID_DUPLICATES = true;
// Should reduce black frames, but slows down the app
const bool USE_OLD_TEXTURES = true;

// What change in zoom should trigger a re-render of the view (0.5 -> 50%)
const float zoomAcceptedChange = 0.25f;
// What change in position should trigger a re-render of the view
const float cameraAcceptedChange = 0.25f;

// Initial position and settings of the camera
long double cameraX = 0;
long double cameraY = 0;
const float cameraSpeed = 500.0f;
long double zoom = 500;
const float zoomSpeed = 1.0f;
int tileWidth = SCREEN_WIDTH / partsX;
int tileHeight = SCREEN_HEIGHT / partsY;

// Multi-threading
std::atomic<int> runningThreads(0);
struct PendingTile {
  int index;
  long double cx, cy, z;
  int generation;
  float maxIterations;
};
std::deque<PendingTile> pendingTiles;
std::unordered_set<int> tilesScheduled; // To avoid duplicates in queue

// DEFINITION OF THE SETS //

// Mandelbrot set
const long double pisqrtpi = PI * sqrt(PI);
const long double pisqrt2 = PI * sqrt(2);
Color getColorFromPoint_Mandelbrot(long double a, long double b, float maxIterations) {
  long double ca = a;
  long double cb = b;

  int n;

  long double aa, bb;
  for (n = 0; (abs(a + b) <= 16) && (n < maxIterations); n++) {
    aa = a * a - b * b + ca;
    b = 2. * a * b + cb;
    a = aa;
  }

  // Coloring
  Color color = BLACK;
  if (n < maxIterations) {
    color.a = 255;
    color.r = ((int)(n * PI)) % 255;
    color.g = ((int)(n * pisqrtpi)) % 255;
    color.b = ((int)(n * pisqrt2)) % 255;
  }

  return color;
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

  switch (i % 6)
  {
  case 0:
    r = v;
    g = t;
    b = p;
    break;
  case 1:
    r = q;
    g = v;
    b = p;
    break;
  case 2:
    r = p;
    g = v;
    b = t;
    break;
  case 3:
    r = p;
    g = q;
    b = v;
    break;
  case 4:
    r = t;
    g = p;
    b = v;
    break;
  case 5:
    r = v;
    g = p;
    b = q;
    break;
  }

  return Color{
      (unsigned char)(r * 255),
      (unsigned char)(g * 255),
      (unsigned char)(b * 255),
      255};
}
Color getColorFromPoint_Julia(long double a, long double b, float maxIterations)
{
  int n = 0;
  long double aa, bb;

  for (; n < maxIterations; ++n)
  {
    if ((a * a + b * b) > 4.0)
      break;
    aa = a * a - b * b + julia_ca;
    bb = 2.0 * a * b + julia_cb;
    a = aa;
    b = bb;
  }

  if (n == maxIterations)
    return BLACK; // Inside set

  // Smooth coloring
  long double zn = sqrt(a * a + b * b);
  long double smooth = n + 1 - log2(log2(zn));

  float hue = (float)(0.95f + 20.0f * smooth / maxIterations); // tweak multiplier
  hue = fmod(hue, 1.0f);                                       // keep hue in [0,1]
  float saturation = 0.8f;
  float value = 1.0f;

  return HSVtoRGB(hue, saturation, value);
}

// Burning ship
Color getColorFromPoint_BurningShip(long double a, long double b, int maxIterations)
{
  long double x = 0, y = 0;
  int n = 0;
  while (x * x + y * y <= 4 && n < maxIterations)
  {
    long double xtemp = x * x - y * y + a;
    y = fabs(2 * x * y) + b;
    x = fabs(xtemp);
    n++;
  }

  float t = (float)n / maxIterations;
  return (n == maxIterations) ? BLACK : Color{(unsigned char)(9 * (1 - t) * t * t * t * 255), (unsigned char)(15 * (1 - t) * (1 - t) * t * t * 255), (unsigned char)(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255), 255};
}

// Tricorn
Color getColorFromPoint_Tricorn(long double a, long double b, int maxIterations)
{
  long double x = 0, y = 0;
  int n = 0;
  while (x * x + y * y <= 4 && n < maxIterations)
  {
    long double xtemp = x * x - y * y + a;
    y = -2 * x * y + b;
    x = xtemp;
    n++;
  }

  float t = (float)n / maxIterations;
  return (n == maxIterations) ? BLACK : Color{(unsigned char)(255 * t), (unsigned char)(255 * (1 - t)), (unsigned char)(128 * t), 255};
}

// Phoenix
Color getColorFromPoint_Phoenix(long double a, long double b, int maxIterations)
{
  // Complex parameters
  long double cRe = a;
  long double cIm = b;

  // Phoenix constant p (can be tweaked for different visuals)
  const long double pRe = -0.5;
  const long double pIm = 0.0;

  long double x = 0.0, y = 0.0;         // z_n
  long double xPrev = 0.0, yPrev = 0.0; // z_{n-1}

  int n = 0;
  while ((x * x + y * y <= 4.0) && n < maxIterations)
  {
    // Complex multiplication: z_n^2
    long double x2 = x * x - y * y;
    long double y2 = 2 * x * y;

    // Add c and p * z_{n-1}
    long double xTemp = x2 + cRe + (pRe * xPrev - pIm * yPrev);
    long double yTemp = y2 + cIm + (pRe * yPrev + pIm * xPrev);

    xPrev = x;
    yPrev = y;
    x = xTemp;
    y = yTemp;

    n++;
  }

  if (n == maxIterations)
    return BLACK;

  // Smooth coloring
  float zn = sqrt(x * x + y * y);
  float smooth = n + 1 - log(log(zn)) / log(2.0);
  float t = smooth / maxIterations;

  // Gradient: smooth rainbow
  unsigned char r = (unsigned char)(9 * (1 - t) * t * t * t * 255);
  unsigned char g = (unsigned char)(15 * (1 - t) * (1 - t) * t * t * 255);
  unsigned char bCol = (unsigned char)(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255);

  return Color{r, g, bCol, 255};
}

// Lyapunov
Color getColorFromPoint_Lyapunov(long double a, long double b, int maxIterations)
{
  // 'a' and 'b' represent rA and rB in the logistic map
  const char *pattern = "AABAB"; // Feel free to change the pattern
  int patternLength = strlen(pattern);

  long double x = 0.5; // Starting value
  long double lyap = 0.0;
  bool stable = true;

  for (int n = 0; n < maxIterations; n++)
  {
    char ch = pattern[n % patternLength];
    long double r = (ch == 'A') ? a : b;
    x = r * x * (1.0 - x);
    if (x <= 0.0 || x >= 1.0)
    {
      stable = false;
      break;
    }
    long double deriv = fabs(r * (1.0 - 2.0 * x));
    if (deriv > 0.0)
      lyap += log(deriv);
  }

  if (!stable)
    return BLACK;

  lyap /= maxIterations;

  // --- Gradient coloring ---
  float t = (float)((lyap + 2.0) / 4.0); // Normalize exponent range ~[-2,2] to [0,1]
  t = fminf(fmaxf(t, 0.0f), 1.0f);       // Clamp

  // Warm fiery gradient: deep red to yellow
  unsigned char r = (unsigned char)(255 * t);
  unsigned char g = (unsigned char)(200 * sqrt(t));
  unsigned char bl = (unsigned char)(30 * (1.0 - t));

  return Color{r, g, bl, 255};
}

// CODE //

// Tile structure
struct Tile
{
  // Lock used to modify the tile in different threads
  std::mutex texMutex;

  // Textures
  RenderTexture2D texture, oldTexture, veryOldTexture;
  int tileX, tileY;

  // Coordinates of the top left and bottom right of the tile, in world space
  long double a1, b1, a2, b2;
  // Coordinates of the camera when the tile was computed
  long double cx, cy, z;

  // Old positions to draw the old texture
  long double olda1, oldb1, olda2, oldb2;
  long double veryolda1, veryoldb1, veryolda2, veryoldb2;
  int generation = 0;

  Color *pixels;
  bool hasComputed = false;
};

// List of all the tiles
std::vector<Tile> tiles(partsX *partsY);

// Compute a tile in the background
void computeTileThread(int tileIndex, long double cx, long double cy, long double z, int generation, float maxIterations)
{
  // Add one to the thread counter
  runningThreads.fetch_add(1, std::memory_order_relaxed);

  // Get the tile
  Tile &tile = tiles[tileIndex];

  // Update generation count
  {
    std::lock_guard<std::mutex> lock(tile.texMutex);
    if (tile.generation <= generation)
    {
      tile.generation = generation;
    }
  }

  // Compute the pixels
  Color *pixels = new Color[tileWidth * tileHeight];
  for (int y = 0; y < tileHeight; y++)
  {
    for (int x = 0; x < tileWidth; x++)
    {
      long double posx = (x + tile.tileX * tileWidth - SCREEN_WIDTH / 2.0) / z + cx;
      long double posy = (y + tile.tileY * tileHeight - SCREEN_HEIGHT / 2.0) / z + cy;

      if (SET == 0) {
        pixels[y * tileWidth + x] = getColorFromPoint_Mandelbrot(posx, posy, maxIterations);
      } else if (SET == 1) {
        pixels[y * tileWidth + x] = getColorFromPoint_Julia(posx, posy, maxIterations);
      } else if (SET == 2) {
        pixels[y * tileWidth + x] = getColorFromPoint_BurningShip(posx, posy, maxIterations);
      } else if (SET == 3) {
        pixels[y * tileWidth + x] = getColorFromPoint_Tricorn(posx, posy, maxIterations);
      } else if (SET == 4) {
        pixels[y * tileWidth + x] = getColorFromPoint_Phoenix(posx, posy, maxIterations);
      } else if (SET == 5) {
        pixels[y * tileWidth + x] = getColorFromPoint_Lyapunov(posx, posy, maxIterations);
      }
    }
  }

  // Lock tile to save computed pixels
  {
    std::lock_guard<std::mutex> lock(tile.texMutex);
    if (tile.generation <= generation)
    {
      tile.generation = generation;
      tile.pixels = pixels;
      tile.hasComputed = true;
      tile.cx = cx;
      tile.cy = cy;
      tile.z = z;
    }
  }

  // Remove one from the thread counter
  runningThreads.fetch_sub(1, std::memory_order_relaxed);
}

// Do a spiral
std::vector<int> getSpiralIndicesOutward(int partsX, int partsY)
{
  std::vector<int> result;
  std::vector<std::vector<bool>> visited(partsY, std::vector<bool>(partsX, false));

  // Starting position (center)
  int startX = partsX / 2;
  int startY = partsY / 2;

  // Direction vectors: right, down, left, up
  int dx[] = {1, 0, -1, 0};
  int dy[] = {0, 1, 0, -1};
  int direction = 0;

  int x = startX, y = startY;
  int steps = 1;

  // Add center point
  result.push_back(x + y * partsX);
  visited[y][x] = true;

  while (result.size() < partsX * partsY)
  {
    for (int i = 0; i < 2; i++)
    { // Move in current direction twice per spiral layer
      for (int step = 0; step < steps; step++)
      {
        x += dx[direction];
        y += dy[direction];

        if (x >= 0 && x < partsX && y >= 0 && y < partsY && !visited[y][x])
        {
          result.push_back(x + y * partsX);
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
std::vector<int> spiralIndicesOutward = getSpiralIndicesOutward(partsX, partsY);
void updateTilesParallel(long double cx, long double cy, long double z, int generation, float maxIterations, long double diffX, long double diffY)
{
  int tileCount = tiles.size();

  if (DETACHED_MODE)
  {
    // Adds the tile to the queue, with all needed information to compute the pixels
    auto scheduleTile = [cx, cy, z, generation, maxIterations](int i)
    {
      PendingTile pendingTile = {i, cx, cy, z, generation, maxIterations};

      // Remove tile from pending list if it was already scheduled (optional, but makes the app faster)
      if (AVOID_DUPLICATES && tilesScheduled.find(i) != tilesScheduled.end())
      {
        for (auto it = pendingTiles.begin(); it != pendingTiles.end(); ++it)
        {
          if (it->index == i)
          {
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
    if (diffX == 0 && diffY == 0)
    {
      for (const int index : spiralIndicesOutward)
      {
        scheduleTile(index);
      }
      return;
    }

    // Change the order of the tiles based on movement direction
    if (diffX >= 0)
    {
      if (diffY >= 0)
      {
        // Top left
        for (int i = 0; i < partsX; ++i)
        {
          for (int j = 0; j < partsY; ++j)
          {
            scheduleTile(j * partsX + i);
          }
        }
      }
      else
      {
        // Bottom left
        for (int i = 0; i < partsX; ++i)
        {
          for (int j = partsY - 1; j >= 0; --j)
          {
            scheduleTile(j * partsX + i);
          }
        }
      }
    }
    else
    {
      if (diffY >= 0)
      {
        // Top right
        for (int i = partsX - 1; i >= 0; --i)
        {
          for (int j = 0; j < partsY; ++j)
          {
            scheduleTile(j * partsX + i);
          }
        }
      }
      else
      {
        // Bottom right
        for (int i = partsX - 1; i >= 0; --i)
        {
          for (int j = partsY - 1; j >= 0; --j)
          {
            scheduleTile(j * partsX + i);
          }
        }
      }
    }
  }
  else
  {
    // Compute all threads at the same time
    std::vector<std::thread> workers;
    for (int i = 0; i < tileCount; ++i)
    {
      workers.emplace_back(computeTileThread, i, cx, cy, z, generation, maxIterations);
    }
    for (auto &t : workers)
    {
      t.join(); // waits for all threads
    }
  }
}

// Main function
int main()
{
  if (FULLSCREEN)
  {
    SetConfigFlags(FLAG_WINDOW_TOPMOST | FLAG_WINDOW_UNDECORATED);
    InitWindow(0, 0, "Fractal Explorer - Multi-threaded");
    HideCursor();
    SCREEN_WIDTH = GetScreenWidth();
    SCREEN_HEIGHT = GetScreenHeight() - 35;
    tileWidth = SCREEN_WIDTH / partsX;
    tileHeight = SCREEN_HEIGHT / partsY;
  }
  else
  {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Fractal Explorer - Multi-threaded");
  }
  SetTargetFPS(TARGET_FPS);

  // Create tile textures
  for (int y = 0; y < partsY; y++)
  {
    for (int x = 0; x < partsX; x++)
    {
      Tile &tile = tiles[y * partsX + x];
      tile.tileX = x;
      tile.tileY = y;
      tile.texture = LoadRenderTexture(tileWidth, tileHeight);
      tile.oldTexture = LoadRenderTexture(tileWidth, tileHeight);
      tile.veryOldTexture = LoadRenderTexture(tileWidth, tileHeight);
    }
  }

  // Needed to detect camera and zoom change
  long double prevCamX = cameraX;
  long double prevCamY = cameraY;
  long double prevZoom = zoom;
  int generation = 0;
  float maxIterations = MAX_ITERATIONS;

  // Make it easier to call the function
  auto customUpdateTilesParallel = [&prevCamX, &prevCamY, &prevZoom, &maxIterations, &generation](long double cx, long double cy, long double z)
  {
    updateTilesParallel(cameraX, cameraY, zoom, generation, maxIterations, prevCamX - cameraX, prevCamY - cameraY);
    prevCamX = cameraX;
    prevCamY = cameraY;
    prevZoom = zoom;
    generation++;
  };

  // First render
  customUpdateTilesParallel(cameraX, cameraY, zoom);

  // Main loop
  while (!WindowShouldClose())
  {
    // Camera movement and zoom
    if (IsKeyDown(KEY_W)) {
      cameraY -= cameraSpeed / zoom / TARGET_FPS;
    } if (IsKeyDown(KEY_S)) {
      cameraY += cameraSpeed / zoom / TARGET_FPS;
    } if (IsKeyDown(KEY_A)) {
      cameraX -= cameraSpeed / zoom / TARGET_FPS;
    } if (IsKeyDown(KEY_D)) {
      cameraX += cameraSpeed / zoom / TARGET_FPS;
    } if (IsKeyDown(KEY_UP)) {
      zoom += zoomSpeed * zoom / TARGET_FPS;
    } if (IsKeyDown(KEY_DOWN)) {
      zoom -= zoomSpeed * zoom / TARGET_FPS;
    } if (IsKeyPressed(KEY_LEFT_SHIFT)) {
      SHOW_PARTS = !SHOW_PARTS;
    }
    // Change number of iterations
    if (IsKeyPressed(KEY_LEFT)) {
      maxIterations -= 100;
      customUpdateTilesParallel(cameraX, cameraY, zoom);
    } if (IsKeyPressed(KEY_RIGHT)) {
      maxIterations += 100;
      customUpdateTilesParallel(cameraX, cameraY, zoom);
    } if (IsKeyPressed(KEY_R)) { // Reset view
      cameraX = 0;
      cameraY = 0;
      zoom = SCREEN_WIDTH / 3;
      customUpdateTilesParallel(cameraX, cameraY, zoom);
    } if (IsKeyPressed(KEY_C)) { // Output camera position and zoom
      std::cout << TextFormat("Zoom: %.36f", (float)zoom) << std::endl;
      std::cout << TextFormat("Camera X: %.36f", (float)cameraX) << std::endl;
      std::cout << TextFormat("Camera Y: %.36f", (float)cameraY) << std::endl;
    }

    // Automatically re-render the fractal if the view moved too much
    float acceptedChange = 1000.f * cameraAcceptedChange / zoom;
    if (IsKeyPressed(KEY_SPACE) || abs(cameraX - prevCamX) >= acceptedChange || abs(cameraY - prevCamY) >= acceptedChange || abs(1 - (zoom / prevZoom)) >= zoomAcceptedChange)
    {
      customUpdateTilesParallel(cameraX, cameraY, zoom);
    }

    // Do the threads
    while (runningThreads.load(std::memory_order_relaxed) < MAX_THREADS && !pendingTiles.empty())
    {
      PendingTile next = pendingTiles.front();
      pendingTiles.pop_front();
      tilesScheduled.erase(next.index);

      Tile &tile = tiles[next.index];
      if (next.generation - tile.generation >= 0)
      { // Check that the tile has not already been computed by a newer generation
        std::thread(computeTileThread, next.index, next.cx, next.cy, next.z, next.generation, next.maxIterations).detach();
      }
    }

    // Iterate trough each tile to check if needed to copy pixels to texture
    for (auto &tile : tiles)
    {
      if (!tile.hasComputed)
      {
        continue;
      }
      {
        std::lock_guard<std::mutex> lock(tile.texMutex);

        // To not get visual glitches
        if (USE_OLD_TEXTURES)
        {
          // Draw oldTexture on veryOldTexture
          BeginTextureMode(tile.veryOldTexture);
          DrawTexturePro(tile.oldTexture.texture,
                         {0, (float)tileHeight, (float)tileWidth, (float)-tileHeight},
                         {0, 0, (float)tileWidth, (float)tileHeight},
                         {0, 0}, 0, WHITE);
          EndTextureMode();
          tile.veryolda1 = tile.olda1;
          tile.veryoldb1 = tile.oldb1;
          tile.veryolda2 = tile.olda2;
          tile.veryoldb2 = tile.oldb2;

          // Draw texture on oldTexture
          BeginTextureMode(tile.oldTexture);
          DrawTexturePro(tile.texture.texture,
                         {0, (float)tileHeight, (float)tileWidth, (float)-tileHeight},
                         {0, 0, (float)tileWidth, (float)tileHeight},
                         {0, 0}, 0, WHITE);
          EndTextureMode();
          tile.olda1 = tile.a1;
          tile.oldb1 = tile.b1;
          tile.olda2 = tile.a2;
          tile.oldb2 = tile.b2;
        }

        // Save the old camera position from when it was computed
        UpdateTexture(tile.texture.texture, tile.pixels);
        tile.a1 = (tile.tileX * tileWidth - SCREEN_WIDTH / 2.0) / tile.z + tile.cx;
        tile.b1 = (tile.tileY * tileHeight - SCREEN_HEIGHT / 2.0) / tile.z + tile.cy;
        tile.a2 = ((tile.tileX + 1) * tileWidth - SCREEN_WIDTH / 2.0) / tile.z + tile.cx;
        tile.b2 = ((tile.tileY + 1) * tileHeight - SCREEN_HEIGHT / 2.0) / tile.z + tile.cy;

        delete[] tile.pixels;
        tile.hasComputed = false;
      }
    }

    // Actual drawing
    BeginDrawing();
    ClearBackground(BLACK);

    // Draw the old textures to stop visual glitches
    if (USE_OLD_TEXTURES)
    {
      // Very old texture
      for (auto &tile : tiles)
      {
        {
          std::lock_guard<std::mutex> lock(tile.texMutex);

          // Calculate the right position to show the old pixels, based on where they were computed
          float startX = (tile.veryolda1 - cameraX) * zoom + SCREEN_WIDTH / 2.0;
          float startY = (tile.veryoldb1 - cameraY) * zoom + SCREEN_HEIGHT / 2.0;
          float endX = (tile.veryolda2 - cameraX) * zoom + SCREEN_WIDTH / 2.0;
          float endY = (tile.veryoldb2 - cameraY) * zoom + SCREEN_HEIGHT / 2.0;

          DrawTexturePro(tile.veryOldTexture.texture,
                         {0, 0, (float)tileWidth, (float)tileHeight},
                         {startX, startY, endX - startX, endY - startY},
                         {0, 0}, 0, WHITE);

          // Only for debug
          if (SHOW_PARTS)
          {
            DrawRectangleLines(startX, startY, endX - startX, endY - startY, GREEN);
          }
        }
      }

      // Old texture
      for (auto &tile : tiles)
      {
        {
          std::lock_guard<std::mutex> lock(tile.texMutex);

          // Calculate the right position to show the old pixels, based on where they were computed
          float startX = (tile.olda1 - cameraX) * zoom + SCREEN_WIDTH / 2.0;
          float startY = (tile.oldb1 - cameraY) * zoom + SCREEN_HEIGHT / 2.0;
          float endX = (tile.olda2 - cameraX) * zoom + SCREEN_WIDTH / 2.0;
          float endY = (tile.oldb2 - cameraY) * zoom + SCREEN_HEIGHT / 2.0;

          DrawTexturePro(tile.oldTexture.texture,
                         {0, 0, (float)tileWidth, (float)tileHeight},
                         {startX, startY, endX - startX, endY - startY},
                         {0, 0}, 0, WHITE);

          // Only for debug
          if (SHOW_PARTS)
          {
            DrawRectangleLines(startX, startY, endX - startX, endY - startY, RED);
          }
        }
      }
    }

    // Draw all tiles
    for (auto &tile : tiles)
    {
      {
        std::lock_guard<std::mutex> lock(tile.texMutex);

        // Calculate the right position to show the pixels, based on where they were computed
        float startX = (tile.a1 - cameraX) * zoom + SCREEN_WIDTH / 2.0;
        float startY = (tile.b1 - cameraY) * zoom + SCREEN_HEIGHT / 2.0;
        float endX = (tile.a2 - cameraX) * zoom + SCREEN_WIDTH / 2.0;
        float endY = (tile.b2 - cameraY) * zoom + SCREEN_HEIGHT / 2.0;

        DrawTexturePro(tile.texture.texture,
                       {0, 0, (float)tileWidth, (float)tileHeight},
                       {startX, startY, endX - startX, endY - startY},
                       {0, 0}, 0, WHITE);

        // Only for debug
        if (SHOW_PARTS)
        {
          DrawRectangleLines(startX, startY, endX - startX, endY - startY, BLUE);
        }
      }
    }

    // Draw UI
    DrawText(TextFormat("Iterations: %.0f", maxIterations), 10, 10, 20, WHITE);
    DrawText(TextFormat("Generation: %.0f", (float)generation), 10, 30, 20, WHITE);
    DrawText(TextFormat("Tiles: %.0f", (float)partsX * partsY), 10, 50, 20, WHITE);

    DrawText(TextFormat("Threads: %.0f", (float)runningThreads.load(std::memory_order_relaxed)), SCREEN_WIDTH - 10 - MeasureText(TextFormat("Threads: %.0f", (float)runningThreads.load(std::memory_order_relaxed)), 20), 10, 20, WHITE);
    DrawText(TextFormat("Queue: %.0f", (float)pendingTiles.size()), SCREEN_WIDTH - 10 - MeasureText(TextFormat("Queue: %.0f", (float)pendingTiles.size()), 20), 30, 20, WHITE);
    DrawText(TextFormat("FPS: %.0f", (float)GetFPS()), SCREEN_WIDTH - 10 - MeasureText(TextFormat("FPS: %.0f", (float)GetFPS()), 20), 50, 20, WHITE);

    DrawText(TextFormat("Camera X: %.15f", (float)cameraX), 10, SCREEN_HEIGHT - 70, 20, WHITE);
    DrawText(TextFormat("Camera Y: %.15f", (float)cameraY), 10, SCREEN_HEIGHT - 50, 20, WHITE);
    DrawText(TextFormat("Zoom: %.2f | %.2f", zoom, zoom / prevZoom), 10, SCREEN_HEIGHT - 30, 20, WHITE);

    DrawText(TextFormat("%.0f x %.0f", (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT), SCREEN_WIDTH - 10 - MeasureText(TextFormat("%.0f x %.0f", (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT), 20), SCREEN_HEIGHT - 30, 20, WHITE);
    EndDrawing();
  }

  // Unload all textures from memory
  for (auto &tile : tiles)
  {
    UnloadRenderTexture(tile.texture);
    UnloadRenderTexture(tile.oldTexture);
    UnloadRenderTexture(tile.veryOldTexture);
  }

  CloseWindow();

  // To then copy and paste if needed
  std::cout << "## Final view ##" << std::endl;
  std::cout << TextFormat("Zoom: %.36f", (float)zoom) << std::endl;
  std::cout << TextFormat("Camera X: %.36f", (float)cameraX) << std::endl;
  std::cout << TextFormat("Camera Y: %.36f", (float)cameraY) << std::endl;

  return 0;
}
