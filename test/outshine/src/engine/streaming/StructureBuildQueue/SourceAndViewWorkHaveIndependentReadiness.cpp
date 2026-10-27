#include "BuildingQueueFixture.h"
#include "Check.h"
#include "Digest.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;

bool QualifyPhysicalSource(BuildingQueueFixture &fixture) {
  auto &prints = fixture.Stack.Footprints();
  CHECK(fixture.Post() == 1, "initial physical input is queued");
  auto ready = fixture.Finish();
  CHECK(ready.size() == 1, "initial qualified physical product lands");
  if (ready.empty()) { return false; }
  fixture.Queue.CommitsLandings(prints, ready);
  prints.BeginRefinement();
  for (int attempt = 0; attempt < 100 && !prints.RefinementComplete(); ++attempt) {
    (void)fixture.Queue.Posts(fixture.Stack,
                              prints,
                              fixture.Eye,
                              fixture.Source,
                              1,
                              StructureBuildQueue::HeightRequirement::FineOnly,
                              std::nullopt,
                              StructureBuildQueue::BuildPurpose::SourceGeometry);
    auto complete = fixture.Queue.NextLandings(fixture.Stack,
                                               prints,
                                               fixture.Eye,
                                               fixture.Source,
                                               1,
                                               StructureBuildQueue::HeightRequirement::FineOnly,
                                               std::nullopt,
                                               StructureBuildQueue::BuildPurpose::SourceGeometry);
    CHECK(complete.has_value(), "source qualification finishes without an error");
    if (!complete) { return false; }
    if (!complete->empty()) { fixture.Queue.CommitsLandings(prints, *complete); }
    (void)fixture.Queue.AwaitSlice(0.02);
  }
  CHECK(prints.RefinementComplete() && fixture.Queue.SourcesComplete(fixture.Stack, prints),
        "accepted physical inputs have a completed source pass");
  return prints.RefinementComplete();
}

bool MissingView(uint32_t /*tile*/) {
  return false;
}

void CheckAcceptedInputIdentity(BuildingQueueFixture &fixture) {
  auto &prints = fixture.Stack.Footprints();
  const auto accepted = *prints.InputOfTile(0);
  const auto products = prints.PinProducts();
  const Generators::Osm::BuildingField::Baked physicalInput{
      .Coordinates = accepted.Coordinates,
      .Prints = products.front().Physical->Prints,
      .SeatSpreadM = products.front().Physical->SeatSpreadM,
      .AcrossM = products.front().Physical->AcrossM,
      .OccupiedCells = accepted.OccupiedCells,
      .CellBounds = accepted.CellBounds,
      .CellMaxHeightM = accepted.CellMaxHeightM,
      .CellShellErrorM = accepted.CellShellErrorM};
  const auto replaceInput = [&](bool qualified, Generators::Osm::BuildingField::BakeInputs bake) {
    prints.ReplaceAcceptance(prints.PrepareAcceptance(0,
                                                      physicalInput,
                                                      accepted.Sources,
                                                      qualified,
                                                      accepted.Vector,
                                                      std::move(bake),
                                                      accepted.HeightRevision,
                                                      accepted.Heights),
                             physicalInput);
  };
  auto changedHeight = accepted.Bake;
  changedHeight.HeightRasterDigest ^= 1;
  replaceInput(true, std::move(changedHeight));
  CHECK(prints.InputOfTile(0)->SourceKey != accepted.SourceKey &&
            !fixture.Queue.SourcesComplete(fixture.Stack, prints),
        "pending view work cannot certify changed physical height content");
  replaceInput(false, accepted.Bake);
  CHECK(!fixture.Queue.SourcesComplete(fixture.Stack, prints),
        "a view task cannot certify an unqualified accepted input");
  replaceInput(true, accepted.Bake);
  CHECK(fixture.Queue.SourcesComplete(fixture.Stack, prints),
        "restoring the exact qualified source restores only physical readiness");
  fixture.Queue.Clear();
  CHECK(fixture.Queue.SourcesComplete(fixture.Stack, prints) &&
            !fixture.Queue.Complete(fixture.Stack, prints, fixture.Eye, MissingView),
        "discarding view work preserves physical inputs and leaves the view explicitly pending");
  prints.BeginRefinement();
  CHECK(!prints.SourceRefinementComplete() && !fixture.Queue.SourcesComplete(fixture.Stack, prints),
        "a fresh physical-source pass still blocks source readiness before any task is queued");
}

int CheckReadiness() {
  BuildingQueueFixture fixture;
  if (!fixture.Open() || !QualifyPhysicalSource(fixture)) { return Report(); }
  auto &prints = fixture.Stack.Footprints();
  const auto revision = prints.Revision();
  const auto *physical = prints.OfTile(0).data();
  size_t posted = 0;
  for (int attempt = 0; attempt < 100 && posted == 0; ++attempt) {
    posted = fixture.Queue.Posts(fixture.Stack,
                                 prints,
                                 fixture.Eye,
                                 fixture.Source,
                                 1,
                                 StructureBuildQueue::HeightRequirement::FineOnly,
                                 std::nullopt,
                                 StructureBuildQueue::BuildPurpose::ViewDetail,
                                 MissingView);
    if (posted == 0) { (void)fixture.Queue.AwaitSlice(0.02); }
  }
  CHECK(posted == 1 && fixture.Queue.Queued() == 1,
        "missing current view creates real independent view work");
  CHECK(prints.SourceRefinementComplete() && fixture.Queue.SourcesComplete(fixture.Stack, prints),
        "unchanged qualified physical inputs remain ready while their view is pending");
  CHECK(!fixture.Queue.Complete(fixture.Stack, prints, fixture.Eye, MissingView),
        "view-independent source readiness cannot certify the missing rendered view");
  CHECK(prints.Revision() == revision && prints.OfTile(0).data() == physical,
        "view demand preserves physical product ownership and semantic revision");
  const auto otherOwner = prints.SnapshotAccepted();
  CHECK(otherOwner.SourceRefinementComplete() &&
            !fixture.Queue.SourcesComplete(fixture.Stack, otherOwner),
        "a view task with a different reservation owner cannot certify a source snapshot");
  CheckAcceptedInputIdentity(fixture);
  return Report();
}
}

int main() {
  return CheckReadiness();
}
