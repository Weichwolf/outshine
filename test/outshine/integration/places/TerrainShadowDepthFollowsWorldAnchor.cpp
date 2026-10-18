#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

bool CaptureTerrainShadow(double anchorZ, std::vector<float> &depth) {
  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
  declaration.DrawsSky = true;
  declaration.ShadowRadiusM = 256;
  declaration.KeyLux = 10000;
  declaration.KeyElevationDeg = 90;
  SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "terrain-only world opens");
  if (!scene) { return false; }
  SubjectMesh mesh;
  mesh.Anchor = {{0, 0, anchorZ}};
  CHECK(renderer.SetSubjectMesh(mesh, error), "empty subjects retain the terrain anchor");
  const std::vector<float> nodes(GroundLattice::kPageNodes, 3.0f);
  const auto page = renderer.PlaceHeightPage(nodes);
  CHECK(page.has_value(), "terrain heights upload");
  if (!page) { return false; }
  std::vector<float> fractions(GroundLattice::kSide);
  for (size_t at = 0; at < fractions.size(); ++at) {
    fractions[at] = static_cast<float>(at) / static_cast<float>(fractions.size() - 1u);
  }
  TerrainTile tile;
  tile.Corners = {{-64, 64, 64, 64, -64, -64, 64, -64}};
  tile.Page = *page;
  tile.LowM = tile.HighM = 3;
  tile.StepE = tile.StepN = 4;
  CHECK(renderer.SetGroundGrid(fractions, error) && renderer.SetTerrainTiles({&tile, 1}, {}, error),
        "terrain-only caster is complete");
  Viewpoint eye;
  eye.EyeM = {{0, 0, anchorZ + 100}};
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 2000;
  scene->Eye(eye);
  renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 256, true, 0);
  CHECK(scene->Draw(error), "terrain casts without a mesh vertex buffer");
  renderer.WaitForGpu();
  CHECK(renderer.ReadShadowAtlas(depth) == ReadState::Ready && !depth.empty(),
        "all four native shadow regions are readable");
  if (depth.size() != size_t(kShadowAtlasPx) * kShadowAtlasPx) { return false; }
  for (size_t region = 0; region < kSunShadowRegions; ++region) {
    const size_t left = (region % 2u) * kShadowTilePx;
    const size_t top = (region / 2u) * kShadowTilePx;
    size_t written = 0;
    bool correctDepth = true;
    for (size_t row = 0; row < size_t(kShadowTilePx); ++row) {
      for (size_t column = 0; column < size_t(kShadowTilePx); ++column) {
        const float value = depth[(top + row) * kShadowAtlasPx + left + column];
        if (value == 0) { continue; }
        ++written;
        correctDepth &= std::abs(value - (0.5f + 3.0f / 1024.0f)) < 2e-6f;
      }
    }
    CHECK(written > 0 && correctDepth, "each region contains the analytical plane depth");
  }
  CHECK(renderer.SetTerrainTiles({}, {}, error), "terrain caster is removed");
  CHECK(scene->Draw(error), "terrain removal renders at the same world anchor");
  renderer.WaitForGpu();
  std::vector<float> cleared;
  CHECK(renderer.ReadShadowAtlas(cleared) == ReadState::Ready && !cleared.empty() &&
            std::ranges::all_of(cleared, [](float value) { return value == 0; }),
        "removing terrain clears every shadow region");
  return true;
}

}

int main() {
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    CHECK(false, "video initializes for terrain shadow translation");
    return Report();
  }
  std::vector<float> origin;
  if (CaptureTerrainShadow(0, origin)) {
    for (const double anchor : {6400000.0, -6400000.0}) {
      std::vector<float> translated;
      CHECK(CaptureTerrainShadow(anchor, translated) && translated == origin,
            "world translation preserves all four terrain shadow footprints and depths");
    }
  }
  SDL_Quit();
  return Report();
}
