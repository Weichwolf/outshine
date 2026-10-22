#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "TerrainRenderer.h"
#include "TerrainResidency.h"
#include "TileGeodesy.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
constexpr float kFlatAslM = 100;
constexpr std::array<float, 2> kDeformedAslM{-1000, 9000}, kAlternatingAslM{-700, 1900};
constexpr int kPageSide = Render::TerrainRenderer::kSide + 2, kMiddleNode = kPageSide / 2;

void CheckSamples(const TerrainResidency &terrain, Data::TileId tile) {
  const auto bounds = terrain.AslBoundsM();
  CHECK(bounds, "completed native terrain provides conservative ASL bounds");
  if (!bounds) { return; }
  bool contained = true;
  constexpr int side = 33;
  for (int row = 0; row < side; ++row) {
    for (int column = 0; column < side; ++column) {
      const Ground::Geo geo = Ground::TileFracToGeo(
          {.X = tile.X + (column + .25) / side, .Y = tile.Y + (row + .75) / side}, tile.Zoom);
      const auto sample =
          terrain.HeightMAt({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg});
      contained &=
          sample && std::isfinite(*sample) && *sample >= (*bounds)[0] && *sample <= (*bounds)[1];
    }
  }
  CHECK(contained, "every sampled published triangle stays inside the metadata-only bound");
}
}

int main() {
  using namespace outshine;
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
      constexpr Data::TileId tile{.Zoom = 16, .X = 32768, .Y = 32768};
      const Ground::Geo anchor =
          Ground::TileFracToGeo({.X = tile.X + .5, .Y = tile.Y + .5}, tile.Zoom);
      const TangentFrame frame = TangentFrame::At(
          {.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg});
      TerrainResidency terrain;
      terrain.Into(&renderer);
      CHECK(!terrain.AslBoundsM(), "unpublished terrain remains unknown");
      Patchwork patch;
      patch.Sheets.push_back(
          {.Tile = tile,
           .Nodes = std::vector<float>(Render::TerrainRenderer::kPageNodes, kFlatAslM),
           .Side = Render::TerrainRenderer::kSide,
           .Virtual = true});
      CHECK(terrain.Publish(patch, frame, error), "uniform virtual terrain publishes");
      CheckSamples(terrain, tile);
      auto &sheet = patch.Sheets.front();
      sheet.Nodes[kMiddleNode * kPageSide + kMiddleNode] = kDeformedAslM[1];
      sheet.Nodes[(kMiddleNode - 1) * kPageSide + kMiddleNode] = kDeformedAslM[0];
      CHECK(terrain.BeginPublish(patch, error), "deformation begins a new publication");
      CHECK(!terrain.AslBoundsM(), "an incomplete publication cannot lend stale bounds");
      CHECK(terrain.Publish(patch, frame, error), "deformation replaces the resident page");
      CheckSamples(terrain, tile);
      const auto deformed = terrain.AslBoundsM();
      CHECK(deformed && (*deformed)[0] <= kDeformedAslM[0] && (*deformed)[1] >= kDeformedAslM[1],
            "bounds include deformation rather than an earlier source height");
      const auto bytes = terrain.HeapBytes();
      sheet.Nodes.assign(sheet.Nodes.size(), 0);
      CHECK(terrain.AslBoundsM() == deformed, "bounds own their published values");
      CHECK(terrain.HeapBytes() == bytes, "bound queries retain no nodes or query allocations");
      sheet.Virtual = false;
      sheet.Postings = 256;
      for (size_t at = 0; at < sheet.Nodes.size(); ++at) {
        sheet.Nodes[at] = kAlternatingAslM[at % 2];
      }
      CHECK(terrain.Publish(patch, frame, error), "nonuniform source-posting terrain publishes");
      CheckSamples(terrain, tile);
      sheet.Nodes.clear();
      CHECK(terrain.Publish(patch, frame, error), "a page without native nodes falls back");
      CHECK(!terrain.HeightMAt(
                {.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg}),
            "an omitted page cannot supply its previous published height");
      CHECK(!terrain.AslBoundsM(), "omitted terrain has no rendered height interval");
      terrain.Clear();
      CHECK(!terrain.AslBoundsM(), "cleared terrain has no stale height bounds");
    }
  }
  SDL_Quit();
  return outshine::Test::Report();
}
