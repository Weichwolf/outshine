#include <cstdint>
#include <cstddef>
#include "TerrainLattice.h"
#include "Check.h"
#include "ChunkSurface.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "streaming/TerrainResidency.h"
#include "TileGeodesy.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr auto kSlopeToleranceM = 0.0001;
constexpr auto kHeightToleranceM = 0.001;
constexpr auto kNearM = 0.01f;
constexpr auto kSlopeFraction = 0.123;
constexpr auto kGroundHeightM = 100;
constexpr auto kRidgeHeightM = 130;
constexpr auto kRidgeColumn = outshine::Render::TerrainLattice::kPageSide / 2;
constexpr auto kPostingIntervals = 255;
constexpr auto kGridSide = outshine::Render::TerrainLattice::kSide;
constexpr auto kPageSide = outshine::Render::TerrainLattice::kPageSide;
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) {
      constexpr int zoom = 16;
      constexpr uint32_t centre = 32768;
      const Ground::Geo anchor =
          Ground::TileFracToGeo({.X = centre + 0.5, .Y = centre + 0.5}, zoom);
      const TangentFrame frame = TangentFrame::At(
          {.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg});
      Patchwork patch;
      patch.Sheets.push_back(
          {.Tile = {.Zoom = zoom, .X = centre, .Y = centre},
           .Nodes = std::vector<float>(Render::TerrainLattice::kPageNodes, kGroundHeightM),
           .Side = Render::TerrainLattice::kSide,
           .Virtual = true});
      TerrainResidency terrain;
      terrain.Into(&renderer);
      CHECK(terrain.Publish(patch, frame, error), "final native heights publish");
      const auto heightM = terrain.HeightMAt(
          {.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg});
      CHECK(heightM && std::abs(*heightM - kGroundHeightM) < kHeightToleranceM,
            "published height is sampled");
      const Ray across{.OriginM = {{-200, 110, 0}}, .Toward = {{1, 0, 0}}};
      CHECK(!terrain.Occludes(across, kNearM, 400, frame), "flat terrain leaves direct path open");
      for (int row = 0; row < kPageSide; ++row) {
        patch.Sheets[0].Nodes[static_cast<size_t>(row) * kPageSide + kRidgeColumn] = kRidgeHeightM;
      }
      CHECK(terrain.Publish(patch, frame, error), "deformed native heights replace the page");
      CHECK(terrain.Occludes(across, kNearM, 400, frame),
            "final deformed ridge blocks without a triangle mesh");
      CHECK(!terrain.Occludes(across, kNearM, kGroundHeightM, frame),
            "terrain beyond source does not block");
      CHECK(!terrain.Occludes(
                {.OriginM = {{-200, 150, 0}}, .Toward = {{1, 0, 0}}}, kNearM, 400, frame),
            "path above the ridge stays open");
      CHECK(!terrain.Occludes(across, 1, 1, frame) &&
                !terrain.Occludes(across, 0, std::numeric_limits<float>::infinity(), frame),
            "empty and invalid intervals do not start unbounded work");
      const size_t before = terrain.HeapBytes();
      for (int query = 0; query < kGroundHeightM; ++query) {
        CHECK(terrain.Occludes(across, kNearM, 400, frame), "repeated native query is stable");
      }
      CHECK(terrain.HeapBytes() == before, "queries allocate no mesh or retained query storage");
      Sheet &sheet = patch.Sheets[0];
      sheet.Virtual = false;
      sheet.Postings = 256;
      for (int row = 1; row <= kGridSide; ++row) {
        for (int col = 1; col <= kGridSide; ++col) {
          sheet.Nodes[static_cast<size_t>(row) * kPageSide + static_cast<size_t>(col)] =
              kGroundHeightM +
              static_cast<float>(Ground::ChunkNodePosting(col - 1, 256, kGridSide)) * 32 /
                  kPostingIntervals;
        }
      }
      CHECK(terrain.Publish(patch, frame, error), "nonuniform posting layout publishes");
      const Ground::Geo sloped =
          Ground::TileFracToGeo({.X = centre + kSlopeFraction, .Y = centre + 0.456}, zoom);
      const auto slopeM = terrain.HeightMAt(
          {.LongitudeDeg = sloped.LongitudeDeg, .LatitudeDeg = sloped.LatitudeDeg});
      CHECK(slopeM && std::abs(*slopeM - (kGroundHeightM + kSlopeFraction * 32)) < kSlopeToleranceM,
            "sampling follows actual posting locations rather than a uniform grid");
      patch.Sheets.clear();
      CHECK(terrain.HeightMAt(
                {.LongitudeDeg = sloped.LongitudeDeg, .LatitudeDeg = sloped.LatitudeDeg}) == slopeM,
            "resident pages own the published values");
      CHECK(!terrain.HeightMAt({.LongitudeDeg = 0, .LatitudeDeg = 89}) &&
                !terrain.HeightMAt({.LongitudeDeg = 20, .LatitudeDeg = 20}),
            "unavailable and polar terrain remain unknown");
      terrain.Clear();
      CHECK(!terrain.HeightMAt(
                {.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg}) &&
                !terrain.Occludes(across, kNearM, 400, frame),
            "cleared terrain has no stale acoustic surfaces");
    }
  }
  SDL_Quit();
  return Report();
}
