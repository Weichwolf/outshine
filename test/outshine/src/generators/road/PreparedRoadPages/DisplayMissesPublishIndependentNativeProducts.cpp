#include "OpenAssetStore.h"
#include "PreparedRoadPageFixture.h"
#include "RoadPageMeshProducer.h"
#include "Check.h"
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kLimit = size_t{1024} * 1024;

RoadPageRecipe Recipe() {
  RoadPageRecipe recipe;
  recipe.Intervals.push_back({.Edge = 7,
                              .Lane = 0,
                              .Stations = {{.EastM = 0}, {.EastM = 1024}},
                              .Surface = {.HalfWidthM = 3,
                                          .Profile = RoadProfile::Simple,
                                          .WearsLinear = {{0.2f, 0.3f, 0.4f}},
                                          .Form = RibbonForm::Surface}});
  return recipe;
}

void CheckCache(PreparedRoadPages &pages) {
  const RoadPageRequest original{
      .Address = {.PlanKey = std::string(64, 'a'),
                  .MaterialKey = std::string(64, 'b'),
                  .MaterialIndex = 0,
                  .Anchor = {.LongitudeDeg = 8, .LatitudeDeg = 49},
                  .Level = 0,
                  .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}}},
      .Parent = {},
      .RequestKey = {},
      .NativeBytesMost = kLimit};
  const RoadPageMeshProducer fineProducer(RoadPageKey(original.Address), Recipe());
  const auto fine = PrepareAndLoadRoadPage(pages, original, &fineProducer);
  CHECK(fine.has_value(), "the original native page remains independently available");
  auto display = original;
  display.Address.UpperModelErrorM = 1;
  const RoadPageMeshProducer displayProducer(RoadPageKey(display.Address), Recipe());
  CHECK(!pages.Prepare(display, &displayProducer),
        "a display miss needs an explicit sampling work budget");
  display.SweepLimits = {.StationsMost = 1, .PoseQueriesMost = 256};
  CHECK(!pages.Prepare(display, &displayProducer),
        "a failed display sweep cannot publish a partial product");
  const auto absent = pages.Load(display.Address, kLimit);
  CHECK(absent && !*absent, "failed production leaves the display key absent");
  display.SweepLimits = {.StationsMost = 8, .PoseQueriesMost = 256};
  const auto coarse = PrepareAndLoadRoadPage(pages, display, &displayProducer);
  CHECK(coarse && fine && coarse->Page->Geometry.Mesh.Index.size() == 18 &&
            fine->Page->Geometry.Mesh.Index.size() == size_t{32} * 18,
        "the cache miss directly creates six display triangles beside the fine reference");
  display.SweepLimits.reset();
  const auto hit = PrepareAndLoadRoadPage(pages, display);
  CHECK(hit && coarse && hit->Page->Geometry.Mesh.Index == coarse->Page->Geometry.Mesh.Index &&
            hit->Page->Geometry.Mesh.PositionM == coarse->Page->Geometry.Mesh.PositionM,
        "a native display hit needs neither sampling budget nor producer");
  const auto fineHit = PrepareAndLoadRoadPage(pages, original);
  CHECK(fineHit && fine &&
            fineHit->Page->Geometry.Mesh.PositionM == fine->Page->Geometry.Mesh.PositionM,
        "publishing a display product preserves the independently keyed original");
  auto closed = Recipe();
  closed.Intervals[0].Surface.Form = RibbonForm::ClosedShell;
  const RoadPageMeshProducer closedProducer(RoadPageKey(display.Address), std::move(closed));
  display.SweepLimits = {.StationsMost = 8, .PoseQueriesMost = 256};
  CHECK(!closedProducer.Produce(display, {}),
        "closed bodies cannot silently use fine geometry under the Upper-only model");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-display-page-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  {
    auto pages = PreparedRoadPages::Open(OpenAssetStore(root.string()));
    CHECK(pages.has_value(), "the test opens a private native asset store");
    if (pages) { CheckCache(**pages); }
  }
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the private store is removed after closing");
  return Report();
}
