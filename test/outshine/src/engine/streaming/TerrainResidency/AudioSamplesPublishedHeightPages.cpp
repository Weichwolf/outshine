#include "Check.h"
#include "ChunkSurface.h"
#include "GroundLattice.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "TerrainResidency.h"
#include "TileGeodesy.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

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
      const Ground::Geo anchor = Ground::TileFracToGeo({centre + 0.5, centre + 0.5}, zoom);
      const TangentFrame frame = TangentFrame::At({anchor.LongitudeDeg, anchor.LatitudeDeg});
      Patchwork patch;
      patch.Sheets.push_back({.Tile = {.Zoom = zoom, .X = centre, .Y = centre},
                              .Nodes = std::vector<float>(Render::GroundLattice::kPageNodes, 100),
                              .Side = Render::GroundLattice::kSide,
                              .Virtual = true});
      TerrainResidency terrain;
      terrain.Into(&renderer);
      CHECK(terrain.Publish(patch, frame, error), "final native heights publish");
      const auto heightM = terrain.HeightMAt({anchor.LongitudeDeg, anchor.LatitudeDeg});
      CHECK(heightM && std::abs(*heightM - 100) < 0.001, "published height is sampled");
      const Ray across{{{-200, 110, 0}}, {{1, 0, 0}}};
      CHECK(!terrain.Occludes(across, 0.01f, 400, frame), "flat terrain leaves direct path open");
      for (int row = 0; row < 35; ++row) {
        patch.Sheets[0].Nodes[static_cast<size_t>(row * 35 + 17)] = 130;
      }
      CHECK(terrain.Publish(patch, frame, error), "deformed native heights replace the page");
      CHECK(terrain.Occludes(across, 0.01f, 400, frame),
            "final deformed ridge blocks without a triangle mesh");
      CHECK(!terrain.Occludes(across, 0.01f, 100, frame), "terrain beyond source does not block");
      CHECK(!terrain.Occludes({{{-200, 150, 0}}, {{1, 0, 0}}}, 0.01f, 400, frame),
            "path above the ridge stays open");
      CHECK(!terrain.Occludes(across, 1, 1, frame) &&
                !terrain.Occludes(across, 0, std::numeric_limits<float>::infinity(), frame),
            "empty and invalid intervals do not start unbounded work");
      const size_t before = terrain.HeapBytes();
      for (int query = 0; query < 100; ++query) {
        CHECK(terrain.Occludes(across, 0.01f, 400, frame), "repeated native query is stable");
      }
      CHECK(terrain.HeapBytes() == before, "queries allocate no mesh or retained query storage");
      Sheet &sheet = patch.Sheets[0];
      sheet.Virtual = false;
      sheet.Postings = 256;
      for (int row = 1; row <= 33; ++row) {
        for (int col = 1; col <= 33; ++col) {
          sheet.Nodes[static_cast<size_t>(row * 35 + col)] =
              100 + static_cast<float>(Ground::ChunkNodePosting(col - 1, 256, 33)) * 32 / 255;
        }
      }
      CHECK(terrain.Publish(patch, frame, error), "nonuniform posting layout publishes");
      const Ground::Geo sloped = Ground::TileFracToGeo({centre + 0.123, centre + 0.456}, zoom);
      const auto slopeM = terrain.HeightMAt({sloped.LongitudeDeg, sloped.LatitudeDeg});
      CHECK(slopeM && std::abs(*slopeM - (100 + 0.123 * 32)) < 0.0001,
            "sampling follows actual posting locations rather than a uniform grid");
      patch.Sheets.clear();
      CHECK(terrain.HeightMAt({sloped.LongitudeDeg, sloped.LatitudeDeg}) == slopeM,
            "resident pages own the published values");
      CHECK(!terrain.HeightMAt({0, 89}) && !terrain.HeightMAt({20, 20}),
            "unavailable and polar terrain remain unknown");
      terrain.Clear();
      CHECK(!terrain.HeightMAt({anchor.LongitudeDeg, anchor.LatitudeDeg}) &&
                !terrain.Occludes(across, 0.01f, 400, frame),
            "cleared terrain has no stale acoustic surfaces");
    }
  }
  SDL_Quit();
  return Report();
}
