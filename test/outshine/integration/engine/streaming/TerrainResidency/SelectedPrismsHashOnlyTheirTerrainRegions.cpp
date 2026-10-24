#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "TerrainRenderer.h"
#include "TerrainResidency.h"
#include "UrbanAreaTerrainPrisms.h"
#include "TileGeodesy.h"
#include "Units.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

void Probe(Render::SceneRenderer &renderer) {
  constexpr Data::TileId first{.Zoom = 16, .X = 32768, .Y = 32768};
  constexpr std::array pages{first,
                             Data::TileId{.Zoom = 16, .X = 32784, .Y = 32768},
                             Data::TileId{.Zoom = 16, .X = 32768, .Y = 32784}};
  constexpr Data::TileId owner{.Zoom = 11, .X = 1024, .Y = 1024};
  const auto centre = Ground::TileFracToGeo({.X = first.X + .5, .Y = first.Y + .5}, first.Zoom);
  const auto frame =
      TangentFrame::At({.LongitudeDeg = centre.LongitudeDeg, .LatitudeDeg = centre.LatitudeDeg});
  Patchwork patchwork;
  for (const auto page : pages) {
    patchwork.Sheets.push_back(
        {.Tile = page,
         .Nodes = std::vector<float>(Render::TerrainRenderer::kPageNodes, 42),
         .Side = Render::TerrainRenderer::kSide,
         .Postings = Render::TerrainRenderer::kSide});
  }
  TerrainResidency terrain;
  terrain.Into(&renderer);
  std::string error;
  CHECK(terrain.Publish(patchwork, frame, error), "all three independent terrain regions publish");
  Generators::Osm::UrbanArea area;
  const double pitch = area.Recipe.ProjectedPitchM;
  area.FirstCell = {
      static_cast<int64_t>(std::floor(kWgs84A * centre.LongitudeDeg * kDeg2Rad / pitch)),
      static_cast<int64_t>(
          std::floor(kWgs84A * std::asinh(std::tan(centre.LatitudeDeg * kDeg2Rad)) / pitch))};
  area.Columns = area.Rows = 2;
  const std::array selected{Generators::Osm::UrbanPatchRange{.Columns = 2, .Rows = 2}};
  constexpr UrbanTerrainIdentityLimits limits{.PatchesMost = 1, .RegionsMost = 1, .PagesMost = 1};
  const auto broad = terrain.SurfaceIdentity(owner, 1);
  CHECK(!broad && broad.error() == TerrainIdentityError::OverBudget,
        "the whole source owner exceeds the same single-page metadata budget");
  const auto identity = UrbanPrismTerrainIdentity(area, selected, terrain, limits);
  CHECK(identity, "selected terrain identity fits without widening that budget");
  patchwork.Sheets[1].Nodes.front() = 99;
  CHECK(terrain.Publish(patchwork, frame, error), "an unrelated page changes");
  const auto unrelated = UrbanPrismTerrainIdentity(area, selected, terrain, limits);
  CHECK(identity && unrelated && *identity == *unrelated,
        "unrelated source-region changes do not invalidate the selected cached game asset");
  patchwork.Sheets[0].Nodes.front() = 100;
  CHECK(terrain.Publish(patchwork, frame, error), "a contributing native page changes");
  const auto changed = UrbanPrismTerrainIdentity(area, selected, terrain, limits);
  CHECK(identity && changed && *identity != *changed,
        "a contributing published page changes the selected terrain identity");
  CHECK(!UrbanPrismTerrainIdentity(area, selected, terrain, {}),
        "metadata work requires explicit nonzero limits");
  terrain.Clear();
  CHECK(!UrbanPrismTerrainIdentity(area, selected, terrain, limits),
        "unpublished terrain cannot be represented by a previous identity");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) { Probe(renderer); }
  }
  SDL_Quit();
  return Report();
}
