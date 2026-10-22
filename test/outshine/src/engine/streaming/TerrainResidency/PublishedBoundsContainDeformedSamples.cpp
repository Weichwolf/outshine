#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "TerrainRenderer.h"
#include "TerrainResidency.h"
#include "TileGeodesy.h"
#include "UrbanAreaTerrainInstances.h"
#include "Geodesy.h"
#include "math/RenderFrame.h"
#include "math/Units.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
constexpr float kFlatAslM = 100;
constexpr double kPositionToleranceM = 1e-6;
constexpr std::array<float, 2> kDeformedAslM{-1000, 9000}, kAlternatingAslM{-700, 1900};
constexpr int kPageSide = Render::TerrainRenderer::kSide + 2, kMiddleNode = kPageSide / 2;

void CheckSelectedInstances(const TerrainResidency &terrain,
                            LongitudeLatitude anchor,
                            const TangentFrame &frame,
                            bool ready) {
  Generators::Osm::UrbanArea area;
  constexpr uint32_t side = 64;
  constexpr Generators::Osm::UrbanPatchRange selected{.Columns = 2, .Rows = 2};
  const double pitch = area.Recipe.ProjectedPitchM;
  area.FirstCell = {
      static_cast<int64_t>(std::floor(kWgs84A * anchor.LongitudeDeg * kDeg2Rad / pitch)),
      static_cast<int64_t>(
          std::floor(kWgs84A * std::asinh(std::tan(anchor.LatitudeDeg * kDeg2Rad)) / pitch))};
  area.Columns = area.Rows = side;
  area.Tiers.assign(static_cast<size_t>(side) * side, 4);
  const auto instances = PrepareUrbanPatchInstances(area, selected, terrain, frame, 4);
  if (!ready) {
    CHECK(!instances && instances.error() == UrbanInstanceError::MissingTerrain,
          "unpublished or changing terrain cannot ground selected urban instances");
    return;
  }
  CHECK(instances && instances->size() == 4,
        "only the selected patch grounds instances despite occupied surrounding cells");
  if (!instances || instances->size() != 4) { return; }
  for (uint32_t row = 0; row < selected.Rows; ++row) {
    for (uint32_t column = 0; column < selected.Columns; ++column) {
      const auto lot = area.LotAt(column, row);
      CHECK(lot, "selected cells own geographic lots");
      if (!lot) { continue; }
      const auto height = terrain.HeightMAt(lot->Centre);
      CHECK(height, "selected lots lie on published terrain");
      if (!height) { continue; }
      Vec3 ecef;
      GeoToEcef({.LongitudeDeg = lot->Centre.LongitudeDeg,
                 .LatitudeDeg = lot->Centre.LatitudeDeg,
                 .HeightM = *height},
                ecef);
      const Vec3 base{RenderFrame::Of(frame.ToLocalPosition(ecef))};
      CHECK(Length((*instances)[row * selected.Columns + column].TransformPoint({{0, 0, 0}}) -
                   base) < kPositionToleranceM,
            "urban bases follow the final published deformation rather than a raw DEM height");
    }
  }
}

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
      const LongitudeLatitude focus{.LongitudeDeg = anchor.LongitudeDeg,
                                    .LatitudeDeg = anchor.LatitudeDeg};
      const TangentFrame frame = TangentFrame::At(focus);
      TerrainResidency terrain;
      terrain.Into(&renderer);
      CHECK(!terrain.AslBoundsM(), "unpublished terrain remains unknown");
      CheckSelectedInstances(terrain, focus, frame, false);
      Patchwork patch;
      patch.Sheets.push_back(
          {.Tile = tile,
           .Nodes = std::vector<float>(Render::TerrainRenderer::kPageNodes, kFlatAslM),
           .Side = Render::TerrainRenderer::kSide,
           .Virtual = true});
      CHECK(terrain.Publish(patch, frame, error), "uniform virtual terrain publishes");
      CheckSamples(terrain, tile);
      CheckSelectedInstances(terrain, focus, frame, true);
      auto &sheet = patch.Sheets.front();
      sheet.Nodes[kMiddleNode * kPageSide + kMiddleNode] = kDeformedAslM[1];
      sheet.Nodes[(kMiddleNode - 1) * kPageSide + kMiddleNode] = kDeformedAslM[0];
      CHECK(terrain.BeginPublish(patch, error), "deformation begins a new publication");
      CHECK(!terrain.AslBoundsM(), "an incomplete publication cannot lend stale bounds");
      CheckSelectedInstances(terrain, focus, frame, false);
      CHECK(terrain.Publish(patch, frame, error), "deformation replaces the resident page");
      CheckSamples(terrain, tile);
      CheckSelectedInstances(terrain, focus, frame, true);
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
      CheckSelectedInstances(terrain, focus, frame, false);
      terrain.Clear();
      CHECK(!terrain.AslBoundsM(), "cleared terrain has no stale height bounds");
    }
  }
  SDL_Quit();
  return outshine::Test::Report();
}
