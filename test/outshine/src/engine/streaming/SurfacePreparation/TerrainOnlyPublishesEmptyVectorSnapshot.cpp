#include "Check.h"
#include "SurfacePreparation.h"
#include "OfflineTransport.h"
#include "Sink.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>

namespace {

class SilentSink final : public outshine::Sink {
public:
  void Number(const char *, double, const char *) override {}

  void Claim(bool, const char *) override {}

  void Near(double, double, double, const char *, const char *) override {}

  void Say(const std::string &) override {}
};

struct TemporaryCache {
  std::filesystem::path Path =
      std::filesystem::temp_directory_path() /
      ("outshine-terrain-only-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ~TemporaryCache() {
    std::error_code error;
    std::filesystem::remove_all(Path, error);
  }
};

}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  TemporaryCache cache;
  SilentSink sink;
  Data::OfflineTransport wire;
  const std::array providers{Data::SourceProvider{.Kind = "terrain"}};
  Tasks sourceCompute(1);
  SurfacePreparation stack;
  const LongitudeLatitude first{.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274};
  const LongitudeLatitude moved{.LongitudeDeg = 8.6159, .LatitudeDeg = 49.3274};
  CHECK(stack.Open({.Shipped = "src/assets", .Cache = cache.Path.string()},
                   providers,
                   first,
                   wire,
                   sourceCompute,
                   sink,
                   nullptr,
                   1.0),
        "explicit terrain-only provider opens without an implicit vector source");
  if (!stack.Opened()) { return Report(); }
  CHECK(stack.FinestZoomOf(Data::DataKind::Elevation) > 0,
        "native geographic terrain retains a regional render grid independent of its cell level");
  CHECK(!stack.HasVectorSource() && stack.VectorZoom() == kFineZoom,
        "the empty vector snapshot uses the ground classification's spatial grid");
  const auto firstAdvanceAt = stack.AdvanceAt(first, {.IngestTilesMost = 1, .VectorRing = 0});
  CHECK(firstAdvanceAt.has_value(), "terrain-only restand publishes an empty vector snapshot");
  const OsmField *vectors = stack.Vectors();
  CHECK(vectors && vectors->Zoom() == kFineZoom && vectors->Features().empty() &&
            vectors->Tiles().size() == 1 && vectors->PendingTiles() == 0 &&
            vectors->SettledWithin(0),
        "a settled zero-feature tile replaces absent source data without vector IO");
  CHECK(!stack.Ground().At(first).AslM(),
        "publishing an empty vector snapshot does not invent missing elevation");
  if (!vectors) { return Report(); }
  const uint64_t firstGeneration = vectors->Generation();
  const int firstX = vectors->CentreX();
  const auto repeated = stack.AdvanceAt(first, {.IngestTilesMost = 1, .VectorRing = 0});
  CHECK(repeated && stack.Vectors()->Generation() == firstGeneration,
        "same-focus restand does not republish an unchanged empty vector snapshot");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!stack.Classes().Complete() && std::chrono::steady_clock::now() < deadline) {
    CHECK(stack.AdvanceAt(first, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
          "pending classification continues to completion at a stationary eye");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(stack.Classes().Complete(), "classification completed before testing resident reuse");
  for (int frame = 0; frame < 60; ++frame) {
    CHECK(stack.AdvanceAt(first, {.IngestTilesMost = 1, .VectorRing = 0}).has_value(),
          "resident world remains usable throughout stationary frames");
    const auto &cost = stack.LastAdvance();
    CHECK(cost.StreetsMs == 0.0 && cost.WaterMs == 0.0 && cost.SettlementMs == 0.0 &&
              stack.Vectors()->Generation() == firstGeneration,
          "unchanged completed inputs skip ingestion and compaction without republishing");
  }
  const auto shifted = stack.AdvanceAt(moved, {.IngestTilesMost = 1, .VectorRing = 0});
  CHECK(shifted && stack.Vectors()->Generation() > firstGeneration &&
            stack.Vectors()->CentreX() != firstX,
        "cross-tile focus change publishes a new empty vector generation");

  const std::array<OsmField::Declared, 1> declared{{
      {.Layer = OsmLayerName(OsmLayer::Streets),
       .Key = "kind",
       .Value = "road",
       .WidthM = 4.0,
       .LatLon = {49.3274, 8.6159, 49.3275, 8.6160}},
  }};
  stack.Declares(declared);
  const auto withFeature = stack.AdvanceAt(moved, {.IngestTilesMost = 1, .VectorRing = 0});
  CHECK(withFeature && stack.Vectors()->Features().size() == 1 &&
            stack.Vectors()->PendingTiles() == 0,
        "scenario-declared features coexist with absence of a remote vector provider");
  stack.Close();
  CHECK(!stack.Opened() && stack.Vectors() == nullptr && !stack.HasVectorSource(),
        "closing the stack retires its empty vector snapshot and capability");
  return Report();
}
