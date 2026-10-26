#include "BuildingField.h"
#include "BuildingPhysicalProduct.h"
#include "BuildingFoundationJob.h"
#include "Check.h"

#include <array>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace {
constexpr auto kStepBudget = 100;
using Field = outshine::Generators::Osm::BuildingField;

struct Owners {
  std::weak_ptr<const ::outshine::Ground::BuildingPhysicalProduct> Physical;
  std::weak_ptr<const outshine::Ground::BuildingGeometry> Coordinates;
};

std::array<Owners, 2> Populate(Field &field) {
  constexpr double kTileStepDeg = 0.01;
  constexpr double kWidthDeg = 0.001;
  field.AnchorAt({{0, 0, 0}});
  for (uint32_t tile = 0; tile < 2; ++tile) {
    const double longitude = static_cast<double>(tile) * kTileStepDeg;
    const auto coordinates = std::make_shared<outshine::Ground::BuildingGeometry>();
    coordinates->OwnCoordinates({0,
                                 longitude,
                                 0,
                                 longitude + kWidthDeg,
                                 kWidthDeg,
                                 longitude + kWidthDeg,
                                 kWidthDeg,
                                 longitude},
                                {});
    const outshine::Ground::BuildingFootprint print{
        .PointCount = 4, .SeatM = 12, .Street = {}, .TerrainCeiling = {}, .TerrainContacts = {}};
    const Field::Baked baked{.Coordinates = coordinates,
                             .Prints = std::span(&print, 1),
                             .SeatSpreadM = {},
                             .AcrossM = {}};
    field.Take(tile);
    auto pending = field.PrepareAcceptance(tile, baked);
    field.CommitAcceptance(std::move(pending), baked);
  }
  const auto pins = field.PinProducts();
  return {{{.Physical = pins[0].Physical, .Coordinates = pins[0].Coordinates},
           {.Physical = pins[1].Physical, .Coordinates = pins[1].Coordinates}}};
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Field field;
  const auto owners = Populate(field);
  const uint64_t revision = field.Revision();
  const auto frame = TangentFrame::At({.LongitudeDeg = 0, .LatitudeDeg = 0});
  std::optional<Generators::BuildingFoundationJob> job{
      std::in_place, frame, revision, field.PinProducts()};
  const auto started = job->Advance({.Generation = revision, .UnitsMost = 1});
  CHECK(started && !*started, "one bounded unit starts a product-owned stamp job");
  const size_t beforeFailedTake = job->HeapBytes();
  CHECK(!job->Take() && job->HeapBytes() == beforeFailedTake,
        "failed transfer preserves partial outputs and product pins");
  const auto resumed = job->Advance({.Generation = revision, .UnitsMost = 1});
  CHECK(resumed && !*resumed, "the same input continues after an incomplete transfer");
  field.ResetDerived();
  CHECK(!owners[0].Physical.expired() && !owners[1].Physical.expired() &&
            !owners[0].Coordinates.expired() && !owners[1].Coordinates.expired(),
        "an interrupted job pins all physical products and their corresponding geometry");
  CHECK(!job->Advance({.Generation = field.Revision(), .UnitsMost = 1}),
        "a changed publication generation refuses continuation of retained products");
  job.reset();
  CHECK(owners[0].Physical.expired() && owners[1].Physical.expired() &&
            owners[0].Coordinates.expired() && owners[1].Coordinates.expired(),
        "aborting the final job releases every pinned physical and coordinate owner");

  const auto completedOwners = Populate(field);
  Field snapshot = field.SnapshotAccepted();
  const uint64_t stableRevision = snapshot.Revision();
  Generators::BuildingFoundationJob complete(frame, stableRevision, snapshot.PinProducts());
  field.ResetDerived();
  bool done = false;
  for (size_t step = 0; step < kStepBudget && !done; ++step) {
    const auto advanced = complete.Advance({.Generation = stableRevision, .UnitsMost = 1});
    CHECK(advanced.has_value(), "stable per-tile spans advance across every interruption");
    if (!advanced) { break; }
    done = *advanced;
  }
  CHECK(done, "pinned ordered tile products finish without a flattened footprint array");
  auto stamps = complete.Take();
  CHECK(stamps && stamps->size() == 2, "each tile contributes exactly its own grounded footprint");
  const auto repeated = complete.Take();
  CHECK(repeated && repeated->empty() && complete.HeapBytes() == 0,
        "repeated transfer returns no second product and retains no heap storage");
  const auto alreadyDone = complete.Advance({.Generation = stableRevision, .UnitsMost = 1});
  CHECK(alreadyDone && *alreadyDone, "the same completed input stays done after transfer");
  CHECK(!complete.Advance({.Generation = stableRevision + 1, .UnitsMost = 1}),
        "completed jobs retain their publication generation check");
  CHECK(!complete.Advance({.Generation = stableRevision + 1, .UnitsMost = 1}),
        "completed transfer preserves generation checks");
  snapshot.ResetDerived();
  CHECK(completedOwners[0].Physical.expired() && completedOwners[1].Physical.expired() &&
            completedOwners[0].Coordinates.expired() && completedOwners[1].Coordinates.expired(),
        "completed transfer releases pins after their final use");
  return Report();
}
