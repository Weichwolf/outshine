#include "SceneGeometry.h"
#include "Check.h"
#include <array>
#include <memory>
#include <semaphore>
#include <thread>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  const auto part = geometry.addPart("worker source", MaterialInstance{});
  CHECK(part && geometry.setPositions(*part, std::array<float, 3>{1, 2, 3}),
        "source owns its attributes");
  SceneGeometry scene;
  scene.SetGeometry(std::move(geometry));
  const std::weak_ptr<const Geometry> weak = scene.Retain();
  std::binary_semaphore ready{0};
  std::binary_semaphore read{0};
  bool observed = false;
  std::jthread worker([source = scene.Retain(), &ready, &read, &observed] {
    ready.release();
    read.acquire();
    const auto positions = source->positionsOf(0);
    observed = positions.size() == 3 && positions[0] == 1 && positions[1] == 2 && positions[2] == 3;
  });
  ready.acquire();
  scene.Clear();
  CHECK(!weak.expired(), "the worker lease keeps the product alive after scene release");
  read.release();
  worker.join();
  CHECK(observed, "the worker reads valid source attributes after the scene is cleared");
  CHECK(weak.expired(), "the product is released after the joined last reader");
  return Report();
}
