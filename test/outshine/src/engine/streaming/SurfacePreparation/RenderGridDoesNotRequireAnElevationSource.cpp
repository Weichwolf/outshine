#include "SurfacePreparation.h"
#include "GroundPatchwork.h"
#include "OfflineTransport.h"
#include "Sink.h"
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>

namespace {
class SilentSink final : public outshine::Sink {
public:
  void Number([[maybe_unused]] const char *name,
              [[maybe_unused]] double value,
              [[maybe_unused]] const char *unit) override {}

  void Claim([[maybe_unused]] bool passed, [[maybe_unused]] const char *claim) override {}

  void Near(double /*got*/,
            double /*want*/,
            double /*within*/,
            const char * /*unit*/,
            const char * /*why*/) override {}

  void Say([[maybe_unused]] const std::string &line) override {}
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-grid-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated empty source cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  {
    SilentSink sink;
    Data::OfflineTransport wire;
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
                     sink,
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
    CHECK(!stack.Ground().At(focus, coverage.Zoom).AslM(),
          "absent elevation data does not become a fabricated ground height");
    CHECK(stack.Pool().Counters().Fetches == 0,
          "source-independent coverage starts no implicit acquisition");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated source cache removed");
  return Report();
}
