#include "game.h"
#include "renderEngine.h"
#include <chrono>
#include <thread>

#define TINYOBJLOADER_IMPLEMENTATION
#include "../tiny_obj_loader.h"

typedef std::chrono::duration<float, std::chrono::seconds::period> fSec;
#define AS_SECONDS(x) std::chrono::duration_cast<fSec>(x)
#define NOW std::chrono::high_resolution_clock::now()
#define WAIT(x) std::this_thread::sleep_for(x)

using namespace std::chrono_literals;

int main() {
  Game game;
  auto lastTp = NOW;
  fSec diff = AS_SECONDS(1ms);
  fSec desiredDiff = AS_SECONDS(17ms);
  while (game.running()) {
    game.update(std::min(diff.count(), 1.0f));

    diff = AS_SECONDS(NOW - lastTp);
    if (diff < desiredDiff) {
      WAIT(diff - desiredDiff);
    }
    lastTp = NOW;
  }
}
