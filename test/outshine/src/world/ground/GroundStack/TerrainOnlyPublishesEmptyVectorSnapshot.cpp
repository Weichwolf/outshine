#include "Check.h"
#include "GroundStack.h"
#include "OfflineTransport.h"
#include "Sink.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

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
  GroundStack stack;
  const LongitudeLatitude first{.LongitudeDeg = 8.5659, .LatitudeDeg = 49.3274};
  const LongitudeLatitude moved{.LongitudeDeg = 8.6159, .LatitudeDeg = 49.3274};
  CHECK(stack.Open({.Shipped = "src/assets", .Cache = cache.Path.string()},
                   providers,
                   first,
                   wire,
                   sink,
                   nullptr,
                   1.0),
        "explicit terrain-only provider opens without an implicit vector source");
  if (!stack.Opened()) { return Report(); }
  CHECK(!stack.HasVectorSource() && stack.VectorZoom() == kFineZoom,
        "the empty vector snapshot uses the ground classification's spatial grid");
  const auto firstRestand = stack.Restand(first, {.IngestTilesMost = 1, .VectorRing = 0});
  CHECK(firstRestand.has_value(), "terrain-only restand publishes an empty vector snapshot");
  const OsmField *vectors = stack.Vectors();
  CHECK(vectors && vectors->Zoom() == kFineZoom && vectors->Features().empty() &&
            vectors->Tiles().size() == 1 && vectors->PendingTiles() == 0 &&
            vectors->SettledWithin(0),
        "a settled zero-feature tile replaces absent source data without vector IO");
  CHECK(!stack.Ingested() && !stack.Classes().Complete(),
        "empty offline terrain cache cannot become a classified world");
  if (!vectors) { return Report(); }
  const uint64_t firstGeneration = vectors->Generation();
  const int firstX = vectors->CentreX();
  const auto repeated = stack.Restand(first, {.IngestTilesMost = 1, .VectorRing = 0});
  CHECK(repeated && stack.Vectors()->Generation() == firstGeneration,
        "same-focus restand does not republish an unchanged empty vector snapshot");
  const auto shifted = stack.Restand(moved, {.IngestTilesMost = 1, .VectorRing = 0});
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
  const auto withFeature = stack.Restand(moved, {.IngestTilesMost = 1, .VectorRing = 0});
  CHECK(withFeature && stack.Vectors()->Features().size() == 1 &&
            stack.Vectors()->PendingTiles() == 0,
        "scenario-declared features coexist with absence of a remote vector provider");
  stack.Close();
  CHECK(!stack.Opened() && stack.Vectors() == nullptr && !stack.HasVectorSource(),
        "closing the stack retires its empty vector snapshot and capability");
  return Report();
}
