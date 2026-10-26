#include "TilePool.h"
#include "ClassificationPreparation.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "RegionSurfaceRecipes.h"
#include "Check.h"
#include <Transport.h>
#include <array>
#include <chrono>
#include <memory>
#include <thread>
#include <string>

namespace {
constexpr double kInsideCoverageLongitudeDeg = 0.001;
constexpr double kOutsideCoverageLongitudeDeg = 0.1;

class NoTransport final : public outshine::Data::Transport {
public:
  outshine::Data::FetchStart Begin(const std::string & /*url*/) override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket /*ticket*/) override {
    return outshine::Data::Wire::Never();
  }

  void Cancel(outshine::Data::Ticket /*ticket*/) override {}
};

bool Complete(outshine::Ground::ClassificationPreparation &field,
              outshine::Ground::TilePool &pool,
              outshine::LongitudeLatitude at) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    if (!field.Update(pool, at)) { return false; }
    if (field.Complete()) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  outshine::Generators::GroundMaterialCatalog materials;
  outshine::Generators::RegionSurfaceRecipes templates;
  CHECK(materials.Load("data/world/ground-materials.json"), "materials load");
  CHECK(templates.Load("data/world/vegetation.json", materials), "classification rules load");
  Data::RawSourceCache::Config cache;
  cache.Using = Data::RawSourceCache::Use::Off;
  Data::RawSourceCache store(cache);
  Data::SourceSet sources(store);
  NoTransport transport;
  TilePool pool({}, sources, transport);
  Tasks compute(1);
  ClassificationPreparation producer;
  producer.SetRecipes(&templates);
  producer.SetVectorSource(false);
  producer.Open(0, 0, compute);
  const std::array<Generators::Osm::OsmField::Declared, 1> forest{
      {{.Layer = "land",
        .Key = "kind",
        .Value = "forest",
        .Area = true,
        .LatLon = {-0.01, -0.01, -0.01, 0.01, 0.01, 0.01, 0.01, -0.01, -0.01, -0.01}}}};
  producer.Declares(forest);
  CHECK(Complete(producer, pool, {}), "producer completes both native grids");
  const auto classes = producer.Read();
  CHECK(classes, "producer publishes a native classification");
  if (!classes) { return Report(); }
  ClassificationPreparation loaded;
  loaded.SetRecipes(&templates);
  CHECK(!loaded.Restore(classes), "closed preparation rejects restoration");
  loaded.Open(1, 1, compute);
  CHECK(!loaded.Restore(classes), "another tangent frame rejects the product");
  loaded.Open(0, 0, compute);
  CHECK(!loaded.Restore({}), "absent native product is not ready");
  CHECK(loaded.Restore(classes), "native grids restore before any source field exists");
  const auto asked = sources.Counters().Asked;
  const auto posted = pool.Counters().Posts;
  CHECK(loaded.Update(pool, {}) && loaded.Complete() && loaded.PendingTiles() == 0,
        "restored grids serve their original camera without source work");
  CHECK(loaded.Update(pool, {.LongitudeDeg = kInsideCoverageLongitudeDeg, .LatitudeDeg = 0}) &&
            loaded.Complete(),
        "small motion stays inside the existing slack");
  CHECK(sources.Counters().Asked == asked && pool.Counters().Posts == posted &&
            loaded.FeaturesHeld() == 0 && loaded.FineSubmits() == 0 && loaded.CoarseSubmits() == 0,
        "restoration performs no source requests, feature assembly or raster jobs");
  CHECK(loaded.Read() == classes && loaded.ReadPublication().Upload,
        "classification and renderer retain a paired immutable product");
  CHECK(loaded.Update(pool, {.LongitudeDeg = kOutsideCoverageLongitudeDeg, .LatitudeDeg = 0}),
        "motion beyond coverage resumes regular source preparation");
  CHECK(!loaded.Complete() && pool.Counters().Posts > posted,
        "far motion cannot keep claiming readiness for the previous area");
  CHECK(classes->Evaluate(0, 0, nullptr, nullptr) ==
            producer.Read()->Evaluate(0, 0, nullptr, nullptr),
        "native ownership survives replacement of the consumer");
  loaded.Open(0, 0, compute);
  CHECK(loaded.Restore(classes), "new session can restore the same native product");
  loaded.Declares({});
  CHECK(!loaded.Complete(), "explicit source changes invalidate restored readiness");
  loaded.Close();
  CHECK(!loaded.Complete() && !loaded.Read(), "closing releases restored publication");
  const auto before = producer.HeapBytes();
  const auto publication = producer.ReadPublication();
  const auto fineSubmits = producer.FineSubmits();
  producer.ReleaseBuildInputs();
  CHECK(producer.Complete() && producer.PendingTiles() == 0 && producer.FeaturesHeld() == 0 &&
            producer.HeapBytes() < before,
        "completed classification releases its source and raster inputs under memory pressure");
  CHECK(producer.Read() == publication.Classes &&
            producer.ReadPublication().Upload == publication.Upload,
        "releasing build inputs preserves the exact paired native and renderer publication");
  CHECK(producer.Update(pool, {}) && producer.Complete() && producer.FineSubmits() == fineSubmits,
        "the completed camera does not recreate discarded build inputs");
  producer.Declares({});
  CHECK(Complete(producer, pool, {}) && producer.Read() != classes &&
            producer.ClassAt({}, nullptr, nullptr) == -1,
        "changed declarations rebuild after release even when field generations repeat");
  CHECK(classes->Evaluate(0, 0, nullptr, nullptr) ==
            publication.Classes->Evaluate(0, 0, nullptr, nullptr),
        "a pinned prior publication remains unchanged after released inputs are rebuilt");
  return Report();
}
