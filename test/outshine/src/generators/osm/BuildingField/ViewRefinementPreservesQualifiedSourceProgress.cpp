#include "BuildingField.h"
#include "Check.h"
#include <cstdint>

int main() {
  using Field = outshine::Generators::Osm::BuildingField;
  using Purpose = Field::RefinementPurpose;
  using namespace outshine::Test;
  Field field;
  constexpr uint32_t tile = 7;
  field.Take(tile);
  const Field::Baked empty{};
  field.CommitAcceptance(field.PrepareAcceptance(tile, empty), empty);
  CHECK(!field.SourceRefinementComplete(), "unqualified source progress starts pending");
  field.BeginRefinement();
  field.AdvanceRefinement();
  CHECK(field.RefinementComplete() && field.SourceRefinementComplete(),
        "completed source qualification also completes the shared traversal");
  const auto revision = field.Revision();
  field.BeginRefinement(Purpose::View);
  CHECK(!field.RefinementComplete() && field.SourceRefinementComplete() &&
            field.Revision() == revision,
        "a new view traversal preserves source progress and semantic revision");
  auto snapshot = field.SnapshotAccepted();
  CHECK(!snapshot.RefinementComplete() && snapshot.SourceRefinementComplete(),
        "snapshots retain their source and view progress independently");
  field.AdvanceRefinement();
  field.RetryRefinement(tile, Purpose::View);
  CHECK(!field.RefinementComplete() && field.SourceRefinementComplete(),
        "retrying view work cannot revoke unchanged qualified sources");
  field.RetryRefinement(tile);
  CHECK(!field.SourceRefinementComplete(), "retrying physical work revokes source completion");
  field.AdvanceRefinement();
  field.BeginRefinement(Purpose::View);
  field.BeginRefinement();
  CHECK(!field.SourceRefinementComplete(), "new physical demand supersedes a pending view");
  snapshot.ResetDerived();
  CHECK(!snapshot.RefinementComplete() && !snapshot.SourceRefinementComplete(),
        "reset cannot retain source completion from an earlier ownership domain");
  return Report();
}
