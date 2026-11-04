#include "Check.h"
#include "ClipmapSurface.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr size_t kBytes = size_t{4} * 1024 * 1024;

ClipmapHeightUpdate Samples(const ClipmapHeightCache &cache, Vec2 camera) {
  auto update = cache.Plan(camera, kBytes);
  CHECK(update.has_value(), "owned sample preparation is admitted");
  if (!update) { Failed(); }
  for (size_t i = 0; i < update->Positions().size(); ++i) {
    const auto at = update->Positions()[i];
    update->Samples()[i] = static_cast<float>(at[0] / 16 + at[1] / 8);
  }
  return std::move(*update);
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  CHECK(!ClipmapSurface::Make(*device, {.HalfCells = 8, .Levels = 3}, 1),
        "combined field admission precedes GPU allocation");
  const auto surface = Require(
      ClipmapSurface::Make(*device, {.HalfCells = 8, .Levels = 3, .ViewRadiusM = 16}, kBytes));
  CHECK(!surface->Complete() && surface->Fields().Rings.empty(),
        "new fields have no claimed coverage or drawable records");
  const auto heights = Samples(surface->Heights(), {{0.5, 0.5}});
  const auto normals = Samples(surface->NormalHeights(), {{0.5, 0.5}});
  CHECK(!surface->Publish(heights, normals, 1) && !surface->Heights().Origin(0),
        "rejected staging cannot mutate either CPU publication");
  Require(surface->Publish(heights, normals, kBytes));
  CHECK(surface->Complete(), "both resolutions publish after the same accepted submission");
  Require(surface->Prepare({.Camera = {{0.5, 0.5}}, .PreviousCamera = {{0.5, 0.5}}}));
  const auto fields = surface->Fields();
  CHECK(fields.Heights && fields.Normals && fields.RingBuffer && fields.Rings.size() == 3,
        "one surface provides all main and shadow resources");
  Require(surface->Prepare({.Camera = {{0.5, 0.5}}, .PreviousCamera = {{0.5, 0.5}}}));
  CHECK(fields.RingBuffer == surface->Fields().RingBuffer,
        "a stationary view reuses the existing ring buffer");
  Require(surface->Prepare(
      {.Camera = {{10, 0}}, .PreviousCamera = {{0.5, 0.5}}, .Shift = {{-10, 0, 0}}}));
  CHECK(surface->LodFocus() == Vec2{},
        "a coarser accepted surface remains valid outside the finest window");
  CHECK(!surface->Prepare({.Camera = {{20, 0}}}),
        "accepted rings must still cover the entire requested view radius");
  CHECK(!surface->Publish(heights, normals, kBytes) && surface->Complete(),
        "stale preparation cannot invalidate a complete surface");
  const auto missingHeights = Samples(surface->Heights(), {{2.5, 2.5}});
  auto missingNormals = Samples(surface->NormalHeights(), {{2.5, 2.5}});
  std::ranges::fill(missingNormals.Samples(), std::numeric_limits<float>::quiet_NaN());
  Require(surface->Publish(missingHeights, missingNormals, kBytes));
  CHECK(!surface->Complete() && surface->Fields().Rings.empty(),
        "unknown source coverage never becomes a drawable flat parent");
  return Report();
}
