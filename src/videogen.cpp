#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <thread>
#include <iostream>

// Own implementations
#include "sets_definition.hpp"


// Constants
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const int MAX_ITERATIONS = 3000;
const int FPS = 24;
const int DURATION = 10; // In seconds

// What to capture
const long double CAMERA_X = -0.685125052928924560546875000000000000L;
const long double CAMERA_Y = 0.314403444528579711914062500000000000L;
const long double TARGET_ZOOM = 86977941057044480.000000000000000000000000000000000000L;
long double zoom = 500;

// How many horizontal and vertical tiles to create
const int TILES_X = 16;
const int TILES_Y = 9;

// Detaches the threads, can speedup the rendering but uses more resources
const bool DETACHED_MODE = true;
const int MAX_THREADS = std::thread::hardware_concurrency();

// Other
const int tileCount = TILES_X * TILES_Y;
const int tileWidth = SCREEN_WIDTH / TILES_X;
const int tileHeight = SCREEN_HEIGHT / TILES_Y;
const int pixelCount = tileWidth * tileHeight;
const int frameCount = FPS * DURATION; // 20 seconds of video
const long double zoomStep = powl(TARGET_ZOOM / zoom, 1.0L / frameCount);

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
std::vector<Frame> frames(frameCount);

// Save a frame
void saveFrameAsPNG(Frame& frame) {
  std::vector<unsigned char> data(SCREEN_WIDTH * SCREEN_HEIGHT * 3); // RGB only
  
  // Loop trough each tile, and copy pixels
  int temp;
  for (int i = 0; i < tileCount; i++) {
    Tile& tile = frame.tiles[i];
    for (int y = 0; y < tileHeight; y++) {
      for (int x = 0; x < tileWidth; x++) {
        int index = y * tileWidth + x;
        temp = (y + tile.tileY * tileHeight) * SCREEN_WIDTH * 3 + (x + tile.tileX * tileWidth) * 3;
        data[temp + 0] = tile.pixels[index].r;
        data[temp + 1] = tile.pixels[index].g;
        data[temp + 2] = tile.pixels[index].b;
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
  std::vector<Color> pixels = std::vector<Color>(pixelCount);
  for (int y = 0; y < tileHeight; y++) {
    for (int x = 0; x < tileWidth; x++) {
      long double posx = (x + tile.tileX * tileWidth - SCREEN_WIDTH / 2.0) / z + cx;
      long double posy = (y + tile.tileY * tileHeight - SCREEN_HEIGHT / 2.0) / z + cy;
      
      pixels[y * tileWidth + x] = getColorFromPoint_Mandelbrot(posx, posy, maxIterations);
    }
  }

  tile.pixels = pixels;
  tile.hasComputed = true;

  // Lock frame to save
  {
    std::lock_guard<std::mutex> lock(frames[generation].frameMutex);
    frames[generation].tilesComputed++;
    if (frames[generation].tilesComputed == tileCount) {
      saveFrameAsPNG(frames[generation]);
      frames[generation].tilesComputed = 0;
    }
  }

  // Remove one from the thread counter
  runningThreads.fetch_sub(1, std::memory_order_relaxed);
}

// Launch all tile updates in parallel
void scheduleFrame(long double cx, long double cy, long double z, int generation, float maxIterations) {
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
  for (int i = 0; i < frameCount; i++) {
    Frame& frame = frames[i];
    frame.generation = i;
    frame.tiles.resize(tileCount);
    
    for (int y = 0; y < TILES_Y; y++) {
      for (int x = 0; x < TILES_X; x++) {
        Tile& tile = frame.tiles[y * TILES_X + x];
        tile.tileX = x;
        tile.tileY = y;
        tile.generation = i;
        tile.pixels.resize(pixelCount);
      }
    }
  }

  // Slowly ZOOM into the camera position, and schedule the frames
  for (int i = 0; i < frameCount; i++) {
    scheduleFrame(CAMERA_X, CAMERA_Y, zoom, i, MAX_ITERATIONS);
    zoom *= zoomStep;
  }


  // Do the threads
  while (!pendingTiles.empty() || runningThreads.load(std::memory_order_relaxed) > 0) {
    // Add the maximum of threads possible
    while (runningThreads.load(std::memory_order_relaxed) < MAX_THREADS && !pendingTiles.empty()) {
      PendingTile next = pendingTiles.front();
      pendingTiles.pop_front();

      Tile& tile = frames[next.generation].tiles[next.index];
      std::thread(computeTileThread, std::ref(tile), next.cx, next.cy, next.z, next.generation, next.maxIterations).detach();
    }
  }

  std::cout << "Done" << std::endl;
  return 0;
}
