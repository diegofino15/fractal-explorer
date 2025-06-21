#include "raylib.h"
#include <thread>
#include <unordered_set>
#include <iostream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


// Constants
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const int FRAMES = 500;
// What set to display | 0 : Mandebrot | 1 : Julia | 2 : Burning ship | 3 : Tricorn | 4 : Phoenix
const int SET = 0;
const int MAX_ITERATIONS = 3000;

// How many horizontal and vertical tiles to create
const int partsX = 16;
const int partsY = 9;

// Detaches the threads, makes the app smoother but can cause a lot of visual glitches at high iterations and big zoom
// If set to false, there are no visual glitches but the app is a lot slower at high iterations
const bool DETACHED_MODE = true;
const int MAX_THREADS = std::thread::hardware_concurrency();

// Camera
long double cameraX = -1.218296527862548828125000000000000000;
long double cameraY = 0.161521255970001220703125000000000000;
long double zoom = 500;
const float zoomSpeed = 1.0f;
const int partWidth = SCREEN_WIDTH / partsX;
const int partHeight = SCREEN_HEIGHT / partsY;

// DEFINITION OF THE SETS //

// Mandelbrot set
const long double pisqrtpi = PI * sqrt(PI);
const long double pisqrt2 = PI * sqrt(2);
Color getColorFromPoint_Mandelbrot(long double a, long double b, float maxIterations) {
  long double ca = a;
  long double cb = b;

  int n;
  
  long double aa, bb;
  for (n = 0; (abs(a+b) <= 16) && (n < maxIterations); n++) {
    aa = a*a - b*b + ca;
    b = 2. * a * b + cb;
    a = aa;
  }

  // Coloring
  Color color = BLACK;
  if (n < maxIterations) {
    color.a = 255;
    color.r = ((int) (n * PI)) % 255;
    color.g = ((int) (n * pisqrtpi)) % 255;
    color.b = ((int) (n * pisqrt2)) % 255;
  }
  
  return color;
}


// CODE //

// Multi-threading
std::atomic<int> runningThreads(0);
struct PendingTile {
  int index;
  long double cx, cy, z;
  int generation;
  float maxIterations;
};
std::deque<PendingTile> pendingTiles;

// Tile structure
struct Tile {
  // Textures
  int tileX, tileY;
  std::vector<Color> pixels;
  bool hasComputed = false;

  // Which frame it belongs to
  int generation = 0;
};

// Frame structure
struct Frame {
  // Lock used to modify the tile in different threads
  std::mutex frameMutex;
  
  // List of tiles
  std::vector<Tile> tiles;

  // Count of the number of tiles that have been computed
  int tilesComputed = 0;

  int generation = 0;
};

// List of all the frames
std::vector<Frame> frames(FRAMES);

// Save a frame
void saveFrameAsPNG(Frame& frame) {
  std::vector<unsigned char> data(SCREEN_WIDTH * SCREEN_HEIGHT * 3); // RGB only
  
  // Loop trough each tile, and copy pixels
  for (int i = 0; i < partsX * partsY; i++) {
    Tile& tile = frame.tiles[i];
    for (int y = 0; y < partHeight; y++) {
      for (int x = 0; x < partWidth; x++) {
        int index = y * partWidth + x;
        data[(y + tile.tileY * partHeight) * SCREEN_WIDTH * 3 + (x + tile.tileX * partWidth) * 3 + 0] = tile.pixels[index].r;
        data[(y + tile.tileY * partHeight) * SCREEN_WIDTH * 3 + (x + tile.tileX * partWidth) * 3 + 1] = tile.pixels[index].g;
        data[(y + tile.tileY * partHeight) * SCREEN_WIDTH * 3 + (x + tile.tileX * partWidth) * 3 + 2] = tile.pixels[index].b;
      }
    }
  }

  char filename[64];
  snprintf(filename, sizeof(filename), "frames/frame%05d.png", frame.generation);
  stbi_write_png(filename, SCREEN_WIDTH, SCREEN_HEIGHT, 3, data.data(), SCREEN_WIDTH * 3);
  std::cout << "Saved frame " << frame.generation << std::endl;
}

// Compute a tile in the background
void computeTileThread(Tile& tile, long double cx, long double cy, long double z, int generation, float maxIterations) {
  // Add one to the thread counter
  runningThreads.fetch_add(1, std::memory_order_relaxed);

  // Compute the pixels
  std::vector<Color> pixels = std::vector<Color>(partWidth * partHeight);
  for (int y = 0; y < partHeight; y++) {
    for (int x = 0; x < partWidth; x++) {
      long double posx = (x + tile.tileX * partWidth - SCREEN_WIDTH / 2.0) / z + cx;
      long double posy = (y + tile.tileY * partHeight - SCREEN_HEIGHT / 2.0) / z + cy;
      
      pixels[y * partWidth + x] = getColorFromPoint_Mandelbrot(posx, posy, maxIterations);
    }
  }

  tile.pixels = pixels;
  tile.hasComputed = true;

  // Lock frame to save
  {
    std::lock_guard<std::mutex> lock(frames[generation].frameMutex);
    frames[generation].tilesComputed++;
  }

  // Remove one from the thread counter
  runningThreads.fetch_sub(1, std::memory_order_relaxed);
}

// Launch all tile updates in parallel
void scheduleFrame(long double cx, long double cy, long double z, int generation, float maxIterations) {
  int tileCount = partsX * partsY;
  
  if (DETACHED_MODE) {
    // Adds the tile to the queue, with all needed information to compute the pixels
    auto scheduleTile = [cx, cy, z, generation, maxIterations](int i) {
      PendingTile pendingTile = { i, cx, cy, z, generation, maxIterations };
      // Add tile to the queue
      pendingTiles.push_back(pendingTile);
    };

    for (int i = 0; i < tileCount; i++) {
      scheduleTile(i);
    }
  } else {
    // Compute all threads at the same time
    std::vector<std::thread> workers;
    for (int i = 0; i < tileCount; ++i) {
      Tile& tile = frames[generation].tiles[i];
      workers.emplace_back(computeTileThread, std::ref(tile), cx, cy, z, generation, maxIterations);
    }
    for (auto& t : workers) { t.join(); } // wait for all threads
  }
}


// Main function
int main() {
  // Init the frames and tiles
  for (int i = 0; i < FRAMES; i++) {
    Frame& frame = frames[i];
    frame.generation = i;
    frame.tiles.resize(partsX * partsY);
    
    for (int y = 0; y < partsY; y++) {
      for (int x = 0; x < partsX; x++) {
        Tile& tile = frame.tiles[y * partsX + x];
        tile.tileX = x;
        tile.tileY = y;
        tile.generation = i;
        tile.pixels.resize(partWidth * partHeight);
      }
    }
  }

  // Slowly zoom into the camera position, and schedule the frames
  for (int i = 0; i < FRAMES; i++) {
    scheduleFrame(cameraX, cameraY, zoom, i, MAX_ITERATIONS);
    zoom += zoomSpeed * zoom / 60;
  }


  // Do the threads
  const int tileCount = partsX * partsY;
  int frameCount = 0;
  while (!pendingTiles.empty() || runningThreads.load(std::memory_order_relaxed) > 0 || frameCount < FRAMES) {
    // Check if some frames are completely rendered
    for (int i = 0; i < FRAMES; i++) {
      Frame& frame = frames[i];
      if (frame.tilesComputed == tileCount) {
        saveFrameAsPNG(frame);
        frame.tilesComputed = 0;
        frameCount++;
      }
    }
    
    // Add the maximum of threads possible
    while (runningThreads.load(std::memory_order_relaxed) < MAX_THREADS && !pendingTiles.empty()) {
      PendingTile next = pendingTiles.front();
      pendingTiles.pop_front();

      Tile& tile = frames[next.generation].tiles[next.index];
      std::thread(computeTileThread, std::ref(tile), next.cx, next.cy, next.z, next.generation, next.maxIterations).detach();
    }
  }

  return 0;
}
