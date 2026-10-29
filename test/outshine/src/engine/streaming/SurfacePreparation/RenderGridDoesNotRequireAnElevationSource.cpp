#include "SurfacePreparation.h"
#include "GroundPatchwork.h"
#include "OfflineSourceTransport.h"
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-grid-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated empty source cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  {

    Data::OfflineSourceTransport wire;
    Tasks compute(1);
    const Data::ProviderRegistry registry;
    Ground::SurfacePreparation stack;
    constexpr LongitudeLatitude focus{.LongitudeDeg = 9, .LatitudeDeg = 47};
    CHECK(stack.Open({.BuiltinData = "data", .Sources = directory, .Assets = {}},
                     std::span<const Data::SourceProvider>{},
                     registry,
                     focus,
                     wire,
                     compute,
                     nullptr,
                     1.0),
          "surface opens with an explicitly empty registry");
    CHECK(stack.FinestZoomOf(Data::DataKind::Elevation) == 0,
          "render raster selection does not invent an elevation provider");
    Around coverage;
    coverage.LongitudeDeg = focus.LongitudeDeg;
    coverage.LatitudeDeg = focus.LatitudeDeg;
    coverage.Zoom = stack.TerrainRenderZoom();
    const auto tiles = PlanPatchworkTiles(coverage);
    CHECK(tiles && !tiles->empty(), "source-independent render grid gives valid coverage");
    const auto beforeQuery = stack.Pool().Counters();
    CHECK(beforeQuery.Posts == 0 && beforeQuery.Fetches == 0,
          "source-independent coverage starts no implicit acquisition");
    CHECK(!stack.Ground().At(focus, coverage.Zoom).AslM(),
          "absent elevation data does not become a fabricated ground height");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated source cache removed");
  return Report();
}
