#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

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
    declaration.KeyLux = 10000;
    declaration.KeyElevationDeg = 45;
    SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "ground world opens");
    if (scene) {
      std::vector<float> nodes(GroundLattice::kPageNodes, 3.0f);
      const auto page = renderer.PlaceHeightPage(nodes);
      CHECK(page.has_value(), "native ground nodes are placed");
      if (!page) { return Report(); }
      std::vector<float> fractions(GroundLattice::kSide);
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
      Viewpoint eye;
      eye.EyeM = {{0, 0, 100}};
      eye.YfovRad = 1;
      eye.ZNearM = 0.1;
      eye.ZFarM = 2000;
      scene->Eye(eye);
      const LightVisibilityStage::Overhead sun{.ToSun = {{0, 0.70710677f, 0.70710677f}},
                                               .Up = {{0, 1, 0}}};
      renderer.SetShadowFrame(sun.ToSun, sun.Up, 256, true);
      CHECK(scene->Draw(error), "a world with no subject vertex buffer casts its ground");
      renderer.WaitForGpu();
      std::vector<float> first;
      CHECK(renderer.ReadShadowAtlas(first) == ReadState::Ready &&
                std::ranges::any_of(first, [](float depth) { return depth > 0.0f; }),
            "terrain has real caster depth independently of subject geometry");
      LightVisibilityStage projection;
      projection.Declare(sun, 256, true);
      projection.Build({{0, 0, -100}});
      const Mat4 &matrix = projection.RegionProjection(0);
      const Vec3 rayPoint{{0, 80, -97}};
      Vec3 clip;
      for (size_t row = 0; row < 3; ++row) {
        clip[row] = matrix[12 + row];
        for (size_t axis = 0; axis < 3; ++axis) {
          clip[row] += matrix[axis * 4 + row] * rayPoint[axis];
        }
      }
      const size_t column = static_cast<size_t>(std::floor((clip[0] * 0.5 + 0.5) * kShadowTilePx));
      const size_t row = static_cast<size_t>(std::floor((0.5 - clip[1] * 0.5) * kShadowTilePx));
      CHECK(
          !first.empty() && first[row * kShadowAtlasPx + column] == 0.0f,
          "a ray outside the analytical flat-sheet footprint has no artificial seam-skirt caster");
      Vec3 planeOrigin;
      for (size_t axis = 0; axis < 3; ++axis) {
        planeOrigin[axis] = matrix[12 + axis] - 97.0 * matrix[8 + axis];
      }
      bool onlyPhysicalSheet = true;
      double worstDepthError = 0;
      for (size_t y = 0; y < size_t(kShadowTilePx); ++y) {
        for (size_t x = 0; x < size_t(kShadowTilePx); ++x) {
          const float depth = first[y * kShadowAtlasPx + x];
          if (depth == 0) { continue; }
          const double planeX =
              (2.0 * (static_cast<double>(x) + 0.5) / kShadowTilePx - 1.0 - planeOrigin[0]) /
              matrix[0];
          const double planeY =
              (1.0 - 2.0 * (static_cast<double>(y) + 0.5) / kShadowTilePx - planeOrigin[1]) /
              matrix[5];
          onlyPhysicalSheet &= std::abs(planeX) <= 64.01 && std::abs(planeY) <= 64.01;
          const double expected = planeOrigin[2] + matrix[2] * planeX + matrix[6] * planeY;
          worstDepthError =
              std::max(worstDepthError, std::abs(static_cast<double>(depth) - expected));
        }
      }
      CHECK(
          onlyPhysicalSheet && worstDepthError < 2e-6,
          "every written texel intersects the physical flat sheet at its analytical reverse depth");
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
