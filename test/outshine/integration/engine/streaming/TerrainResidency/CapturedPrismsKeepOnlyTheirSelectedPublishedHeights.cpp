#include "TerrainPublicationFixture.h"
#include "TerrainLattice.h"
#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "streaming/TerrainResidency.h"
#include "UrbanAreaTerrainPrisms.h"
#include "TileGeodesy.h"
#include "Geodesy.h"
#include "Sha256.h"
#include "Units.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace {
constexpr auto kHeightToleranceM = 1e-6;

using namespace outshine;
using namespace outshine::Test;
namespace Osm = Generators::Osm;

std::shared_ptr<const Osm::UrbanArea> AreaAt(Data::TileId tile) {
  const auto centre = Ground::TileFracToGeo({.X = tile.X + .5, .Y = tile.Y + .5}, tile.Zoom);
  auto area = std::make_shared<Osm::UrbanArea>();
  const double pitch = area->Recipe.ProjectedPitchM;
  area->FirstCell = {
      static_cast<int64_t>(std::floor(kWgs84A * centre.LongitudeDeg * kDeg2Rad / pitch)),
      static_cast<int64_t>(
          std::floor(kWgs84A * std::asinh(std::tan(centre.LatitudeDeg * kDeg2Rad)) / pitch))};
  area->Columns = area->Rows = 2;
  area->Tiers = {1, 0, 2, 4};
  return area;
}

void CheckCapture(Render::SceneRenderer &renderer) {
  constexpr Data::TileId tile{.Zoom = 16, .X = 32768, .Y = 32768};
  constexpr double initialHeight = 42;
  constexpr double replacementHeight = 1200;
  const auto area = AreaAt(tile);
  const std::array selected{Osm::UrbanPatchRange{.Columns = 2, .Rows = 2}};
  Patchwork patch;
  patch.Sheets.push_back(
      {.Tile = tile,
       .Nodes = std::vector<float>(Render::TerrainLattice::kPageNodes, initialHeight),
       .Side = Render::TerrainLattice::kSide,
       .Postings = Render::TerrainLattice::kSide});
  TerrainResidency terrain;
  terrain.Into(&renderer);
  const auto frame = TangentFrame::At({});
  std::string error;
  CHECK(PublishTerrain(terrain, patch, frame, error), "the original terrain is published");
  const auto request = Sha256Hex("captured-prism/old-content/selection");
  const auto factory = CaptureUrbanPrismFactory(area, selected, terrain, request, 4);
  CHECK(factory, "a cold request captures only selected native heights and owns its source");
  CHECK(!CaptureUrbanPrismFactory(area, selected, terrain, request, 3),
        "a short dense-sample budget refuses capture before generation");
  CHECK(!CaptureUrbanPrismFactory(area, selected, terrain, request, 4, {.BytesMost = 192}),
        "the output byte budget also bounds the number of captured inputs");
  std::stop_source canceled;
  canceled.request_stop();
  CHECK(!CaptureUrbanPrismFactory(area, selected, terrain, request, 4, {}, canceled.get_token()),
        "canceled source capture never retains a new pending job");
  patch.Sheets.front().Nodes.assign(Render::TerrainLattice::kPageNodes, replacementHeight);
  CHECK(PublishTerrain(terrain, patch, frame, error), "the live terrain can change after capture");
  terrain.Clear();
  if (factory) {
    const auto package = (*factory)(Osm::kUrbanPrismBytesMost);
    CHECK(package, "the frozen factory runs without querying the replaced terrain");
    if (package) {
      const auto restored = Osm::DecodeUrbanPrisms(package->Bytes);
      CHECK(restored, "the generated complete package can be decoded");
      if (restored) {
        const auto &shape = restored->Geometry.Patches.front();
        const auto &placement = restored->Placements.front();
        const auto cell = restored->Geometry.Samples->front();
        const Vec3 position = placement.EcefFromLocal.TransformPoint(
            {{shape.PitchX * .5, cell.Lower, shape.PitchZ * .5}});
        const auto geographic = GeoFromEcef(position);
        CHECK(geographic && std::abs(geographic->HeightM - initialHeight) <=
                                placement.MaxCornerErrorM + kHeightToleranceM,
              "ready geometry retains captured published terrain rather than later live values");
      }
    }
    CHECK(!(*factory)(1), "a worker's smaller output budget refuses before building geometry");
  }
  terrain.Clear();
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
    if (scene) { CheckCapture(renderer); }
  }
  SDL_Quit();
  return Report();
}
