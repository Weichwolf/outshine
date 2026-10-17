#include "SceneGeometry.h"
#include "HeapProbe.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

Geometry Triangle(const char *name, float offset = 0.0F) {
  Geometry geometry;
  const auto part = geometry.addPart(name, MaterialInstance{});
  CHECK(part, "a native part is created");
  if (!part) { return geometry; }
  CHECK(geometry.setPositions(
            *part, std::array{offset, 0.0F, 0.0F, offset + 1, 0.0F, 0.0F, offset, 1.0F, 0.0F}),
        "a native triangle owns its positions");
  CHECK(geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "a native triangle owns its indices");
  return geometry;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry = Triangle("large native world");
  const std::vector<float> positions(size_t{3} * 262144, 0.0F);
  CHECK(geometry.setPositions(0, positions), "native fixture has a multi-megabyte position stream");
  const float *const owned = geometry.positionsOf(0).data();
  SceneGeometry scene;
  const auto revision = scene.Revision();
  const size_t before = HeapProbe::LiveBytes();
  scene.SetGeometry(std::move(geometry));
  const size_t after = HeapProbe::LiveBytes();
  CHECK(after <= before, "ownership transfer allocates no second geometry or asset snapshot");
  CHECK(scene.HasGeometry() && scene.Revision() == revision + 1,
        "publishing native geometry increments its revision once");
  CHECK(scene.Snapshot().positionsOf(0).data() == owned,
        "the moved position storage remains the published snapshot");
  scene.Clear();
  CHECK(!scene.HasGeometry() && scene.Snapshot().parts() == 0 && scene.Revision() == revision + 2,
        "clear releases the native snapshot and increments its revision");
  return Report();
}
