#include "TilePool.h"
#include "ClassificationPreparation.h"
#include <SourceTransport.h>
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "RegionSurfaceRecipes.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>

namespace {
class NoTransport final : public outshine::Data::SourceTransport {
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
              outshine::Ground::TilePool &pool) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  do {
    if (!field.Update(pool, outshine::LongitudeLatitude{})) { return false; }
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
  if (!templates.Ready()) { return Report(); }
  Data::RawSourceCache store({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  Data::SourceSet sources(store);
  NoTransport transport;
  TilePool pool({}, sources, transport);
  Tasks compute(1);
  ClassificationPreparation field;
  field.SetRecipes(&templates);
  field.Open(0, 0, compute);
  std::array<::outshine::Generators::Osm::OsmField::Declared, 1> area{
      {{.Layer = "land",
        .Key = "kind",
        .Value = "forest",
        .Area = true,
        .LatLon = {-0.01, -0.01, -0.01, 0.01, 0.01, 0.01, 0.01, -0.01, -0.01, -0.01}}}};
  field.Declares(area);
  CHECK(Complete(field, pool), "initial generation builds both tiers");
  const auto *rule = templates.Find("land", "forest");
  CHECK(rule != nullptr, "forest rule exists");
  if (rule == nullptr) { return Report(); }
  CHECK(field.ClassAt({}, nullptr, nullptr) == rule->Tpl,
        "initial area classifies camera position");
  const auto firstPublication = field.ReadPublication();
  CHECK(firstPublication.Classes && firstPublication.Upload &&
            firstPublication.Classes == field.Read(),
        "native CPU snapshot and prepared renderer upload publish together");
  const auto fine = field.FineSubmits();
  const auto coarse = field.CoarseSubmits();
  area[0].Layer = "excluded-test-layer";
  field.Declares(area);
  CHECK(Complete(field, pool), "empty generation completes without moving camera");
  CHECK(field.FineSubmits() > fine && field.CoarseSubmits() > coarse,
        "empty generation rebuilds both tiers");
  CHECK(field.ClassAt({}, nullptr, nullptr) == -1,
        "empty generation removes previous area classification");
  const auto emptyPublication = field.ReadPublication();
  CHECK(emptyPublication.Classes && emptyPublication.Upload &&
            emptyPublication.Upload != firstPublication.Upload,
        "empty source replacement publishes a new paired renderer product");
  CHECK(firstPublication.Classes->Evaluate(0, 0, nullptr, nullptr) == rule->Tpl,
        "held prior native publication stays usable after renderer replacement");
  std::array<::outshine::Generators::Osm::OsmField::Declared, 1> street{
      {{.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .LatLon = {0, -0.001, 0, 0.001}}}};
  field.Declares(street);
  CHECK(field.Update(pool, LongitudeLatitude{}).has_value(), "first road generation is submitted");
  const auto submitted = field.FineSubmits();
  CHECK(submitted > fine, "road generation starts a fine job");
  street[0].LatLon = {0.02, -0.001, 0.02, 0.001};
  field.Declares(street);
  CHECK(Complete(field, pool), "superseding generation completes");
  CHECK(field.FineSubmits() > submitted, "superseded job triggers a new fine build");
  CHECK(field.ClassAt({}, nullptr, nullptr) == -1,
        "completion cannot expose a superseded road through the camera");

  field.Open(0, 0, compute);
  area[0].Layer = "land";
  field.Declares(area);
  const LongitudeLatitude start{.LongitudeDeg = 0.01, .LatitudeDeg = 0};
  const LongitudeLatitude acrossFineBoundary{.LongitudeDeg = 0.026, .LatitudeDeg = 0};
  CHECK(field.Update(pool, start) && field.AwaitBuild(5), "initial fine raster is ready");
  const auto release = std::make_shared<std::binary_semaphore>(0);
  CHECK(compute.PostDetached([release] { release->acquire(); }), "coarse work waits for motion");
  CHECK(field.Update(pool, start) && field.Update(pool, acrossFineBoundary),
        "motion changes the fine tile while the coarse job is queued");
  release->release();
  CHECK(field.AwaitBuild(5) && field.Update(pool, acrossFineBoundary),
        "completion can reuse the coarse raster and start the new fine raster");
  CHECK(field.AwaitBuild(5) && field.Update(pool, acrossFineBoundary) && field.Complete(),
        "crossing a fine tile publishes after the new fine raster finishes");
  CHECK(field.ClassAt({}, nullptr, nullptr) == rule->Tpl &&
            field.ClassAt(acrossFineBoundary, nullptr, nullptr) == -1,
        "the paired product covers both the coarse forest and the new fine camera position");
  return Report();
}
