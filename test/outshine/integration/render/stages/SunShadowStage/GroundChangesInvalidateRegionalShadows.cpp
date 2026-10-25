#include "TerrainLattice.h"
#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr double kKeyLux = 10000;
constexpr double kKeyElevationDeg = 45;
constexpr double kEyeHeightM = 100;
constexpr double kNearM = 0.1;
constexpr double kFarM = 2000;

void CheckPhysicalSheet(const std::vector<float> &first, const outshine::Mat4 &matrix) {
  using namespace outshine;
  using namespace outshine::Render;
  constexpr double kEyeToSheetM = 97.0;
  constexpr double kSheetEdgeLimitM = 64.01;
  constexpr double kDepthTolerance = 2e-6;
  const Vec3 rayPoint{{0, 80, -97}};
  Vec3 clip;
  for (size_t row = 0; row < 3; ++row) {
    clip[row] = matrix[12 + row];
    for (size_t axis = 0; axis < 3; ++axis) {
      clip[row] += matrix[axis * 4 + row] * rayPoint[axis];
    }
  }
  const auto column = static_cast<size_t>(std::floor((clip[0] * 0.5 + 0.5) * kShadowTilePx));
  const auto row = static_cast<size_t>(std::floor((0.5 - clip[1] * 0.5) * kShadowTilePx));
  CHECK(!first.empty() && first[row * kShadowAtlasPx + column] == 0.0f,
        "a ray outside the analytical flat-sheet footprint has no artificial seam-skirt caster");
  Vec3 planeOrigin;
  for (size_t axis = 0; axis < 3; ++axis) {
    planeOrigin[axis] = matrix[12 + axis] - kEyeToSheetM * matrix[8 + axis];
  }
  bool onlyPhysicalSheet = true;
  double worstDepthError = 0;
  for (size_t y = 0; y < static_cast<size_t>(kShadowTilePx); ++y) {
    for (size_t x = 0; x < static_cast<size_t>(kShadowTilePx); ++x) {
      const float depth = first[y * kShadowAtlasPx + x];
      if (depth == 0) { continue; }
      const double planeX =
          (2.0 * (static_cast<double>(x) + 0.5) / kShadowTilePx - 1.0 - planeOrigin[0]) / matrix[0];
      const double planeY =
          (1.0 - 2.0 * (static_cast<double>(y) + 0.5) / kShadowTilePx - planeOrigin[1]) / matrix[5];
      onlyPhysicalSheet &=
          std::abs(planeX) <= kSheetEdgeLimitM && std::abs(planeY) <= kSheetEdgeLimitM;
      const double expected = planeOrigin[2] + matrix[2] * planeX + matrix[6] * planeY;
      worstDepthError = std::max(worstDepthError, std::abs(static_cast<double>(depth) - expected));
    }
  }
  CHECK(onlyPhysicalSheet && worstDepthError < kDepthTolerance,
        "every written texel intersects the physical flat sheet at its analytical reverse depth");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for ground shadows");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
    declaration.DrawsSky = true;
    declaration.ShadowRadiusM = 256;
    declaration.KeyLux = kKeyLux;
    declaration.KeyElevationDeg = kKeyElevationDeg;
    SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "ground world opens");
    if (scene) {
      std::vector<float> nodes(TerrainLattice::kPageNodes, 3.0f);
      const auto page = renderer.PlaceHeightPage(nodes);
      CHECK(page.has_value(), "native ground nodes are placed");
      if (!page) { return Report(); }
      std::vector<float> fractions(TerrainLattice::kSide);
      for (size_t at = 0; at < fractions.size(); ++at) {
        fractions[at] = static_cast<float>(at) / static_cast<float>(fractions.size() - 1u);
      }
      TerrainTile tile;
      tile.Corners = {{-64, 64, 64, 64, -64, -64, 64, -64}};
      tile.Page = *page;
      tile.LowM = tile.HighM = 3;
      tile.StepE = tile.StepN = 4;
      CHECK(renderer.SetGroundGrid(fractions, error) &&
                renderer.SetTerrainTiles({&tile, 1}, {}, error),
            "ground tile is complete");
      const bool uploadsReady = renderer.Settle(error);
      CHECK(uploadsReady, "offline terrain uploads complete before frame testing");
      if (!uploadsReady) { return Report(); }
      Viewpoint eye;
      eye.EyeM = {{0, 0, kEyeHeightM}};
      eye.YfovRad = 1;
      eye.ZNearM = kNearM;
      eye.ZFarM = kFarM;
      scene->Eye(eye);
      const SunShadowStage::Overhead sun{.ToSun = {{0, 0.70710677f, 0.70710677f}},
                                         .Up = {{0, 1, 0}}};
      renderer.SetShadowFrame(sun.ToSun, sun.Up, 256, true, 0);
      const bool drawn = scene->Draw(error);
      CHECK(drawn, "a world with no subject vertex buffer casts its ground");
      if (!drawn) { return Report(); }
      renderer.WaitForGpu();
      std::vector<float> first;
      const bool completeAtlas = renderer.ReadShadowAtlas(first) == ReadState::Ready &&
                                 first.size() == size_t{kShadowAtlasPx} * kShadowAtlasPx;
      CHECK(completeAtlas && std::ranges::any_of(first, [](float depth) { return depth > 0.0f; }),
            "terrain has real caster depth independently of subject geometry");
      if (!completeAtlas) { return Report(); }
      SunShadowStage projection;
      projection.Declare(sun, 256, true);
      projection.Build({{0, 0, -kEyeHeightM}});
      const Mat4 &matrix = projection.RegionProjection(0);
      CheckPhysicalSheet(first, matrix);
      CHECK(renderer.SetTerrainTiles({}, {}, error),
            "ground is removed without replacing subjects");
      CHECK(scene->Draw(error), "the modified ground world renders");
      renderer.WaitForGpu();
      std::vector<float> second;
      CHECK(renderer.ReadShadowAtlas(second) == ReadState::Ready && !second.empty() &&
                std::ranges::all_of(second, [](float depth) { return depth == 0.0f; }),
            "terrain changes invalidate and clear the submitted shadow cache");
    }
  }
  SDL_Quit();
  return Report();
}
