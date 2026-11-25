#include "Check.h"
#include "ClipmapSurface.h"
#include "TerrainRenderer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr size_t kBytes = size_t{4} * 1024 * 1024;

ClipmapHeightUpdate Samples(const ClipmapHeightCache &cache, Vec2 camera) {
  auto update = cache.Plan(camera, kBytes);
  CHECK(update.has_value(), "camera height preparation is admitted");
  if (!update) { Failed(); }
  std::vector<outshine::Vec2> points(update->Samples().size());
  CHECK(update->FillPositions(0, points), "sample coordinates fit the planned bands");
  for (size_t i = 0; i < points.size(); ++i) {
    const auto at = points[i];
    update->Samples()[i] = static_cast<float>(at[0] / 16 + at[1] / 8);
  }
  return std::move(*update);
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  TerrainResidency resident;
  TerrainRenderer ground(resident);
  std::array<ColourTarget, 5> targets;
  for (auto &target : targets) { target.Texels = Format::Rgba32Float; }
  Require(
      ground.Configure(*device,
                       {.NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4},
                       targets));
  const auto surface =
      Require(ground.CreateClipmaps({.HalfCells = 8, .Levels = 3, .ViewRadiusM = 28}, kBytes));
  const auto initial = ground.Generation();
  const auto heights = Samples(surface->Heights(), {{0.5, 0.5}});
  const auto normals = Samples(surface->NormalHeights(), {{0.5, 0.5}});
  Require(surface->Publish(heights, normals, kBytes));
  FrameContext frame;
  frame.Mvp[0] = frame.Mvp[5] = frame.Mvp[10] = 1.0f / 64;
  frame.PreViewTranslation = {{-0.5, 0, 0.5}};
  frame.PrevPreViewTranslation = frame.PreViewTranslation;
  Require(ground.PrepareClipmaps(frame, {}));
  CHECK(ground.Generation() != initial && ground.Drawn() == 3,
        "preload selects the new representation before a shadow key is captured");
  const auto counts = Require(ground.ReadDrawArguments());
  CHECK(counts.DirectTriangles == surface->Triangles() && counts.SkirtInstances == 0,
        "diagnostics report actual clipmap topology instead of obsolete page draws");
  Require(ground.SetInstances({}, {}));
  Require(ground.PrepareClipmaps(frame, {}));
  CHECK(ground.Triangles() == surface->Triangles() && ground.Drawn() == 3,
        "legacy page-list updates preserve the independently accepted clipmap surface");
  const auto prepared = ground.Generation();
  frame.Mvp[0] = 0.5f;
  frame.PreViewTranslation[1] = -4;
  Require(ground.PrepareClipmaps(frame, {}));
  CHECK(ground.Generation() == prepared && surface->Shift()[1] == -4,
        "camera rotation and vertical translation preserve world geometry and update its frame");
  CHECK(ground.Triangles() < surface->Triangles(),
        "view selection reduces submitted topology before cached shadow preparation");
  frame.Mvp[0] = 1.0f / 64;
  frame.PreViewTranslation[0] = -7;
  CHECK(!surface->Covers({{7, 0.5}}), "camera movement requires replacement coverage");
  Require(ground.PrepareClipmaps(frame, {}));
  CHECK(ground.Generation() == prepared && ground.Drawn() == 3 && surface->Shift()[0] == -7,
        "camera motion preserves the accepted LOD surface until replacement fields are published");
  const auto nextHeights = Samples(surface->Heights(), {{8.5, 0.5}});
  const auto nextNormals = Samples(surface->NormalHeights(), {{8.5, 0.5}});
  Require(surface->Publish(nextHeights, nextNormals, kBytes));
  Require(ground.PrepareClipmaps(frame, {}));
  CHECK(ground.Generation() != prepared,
        "changed published geometry invalidates shadows before raster selection");
  return Report();
}
