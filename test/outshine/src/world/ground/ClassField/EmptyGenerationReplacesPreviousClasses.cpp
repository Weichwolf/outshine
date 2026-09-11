#include "ClassField.h"
#include "Transport.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "VegetationTemplates.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <thread>

namespace {
class NoTransport final : public outshine::Data::Transport {
public:
  outshine::Data::Ticket Begin(const std::string &) override {
    return outshine::Data::Ticket::None;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Never();
  }

  void Cancel(outshine::Data::Ticket) override {}
};

bool Complete(outshine::Ground::ClassField &field, outshine::Ground::TilePool &pool) {
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
  GroundMaterials materials;
  VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "materials load");
  CHECK(templates.Load("src/assets/world/vegetation.json", materials), "classification rules load");
  if (!templates.Ready()) { return Report(); }
  Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
  Data::SourceSet sources(store);
  NoTransport transport;
  TilePool pool({}, sources, transport);
  ClassField field;
  field.SetVegetation(&templates);
  field.Open(0, 0);
  std::array<OsmField::Declared, 1> area{
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
  const auto fine = field.FineSubmits();
  const auto coarse = field.CoarseSubmits();
  area[0].Layer = "excluded-test-layer";
  field.Declares(area);
  CHECK(Complete(field, pool), "empty generation completes without moving camera");
  CHECK(field.FineSubmits() > fine && field.CoarseSubmits() > coarse,
        "empty generation rebuilds both tiers");
  CHECK(field.ClassAt({}, nullptr, nullptr) == -1,
        "empty generation removes previous area classification");
  std::array<OsmField::Declared, 1> street{{{.Layer = "streets",
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
  return Report();
}
