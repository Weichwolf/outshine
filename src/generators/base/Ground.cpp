#include "Ground.h"
#include <optional>

namespace outshine::Generators {

std::optional<Ground> Ground::Of(const Tile &region, const Snapshot &snapshot) {
  return Of(region, snapshot, Detail::Fine);
}

std::optional<Ground> Ground::Of(const Tile &region, const Snapshot &snapshot, Detail coarseness) {
  if (!snapshot.Patch || !snapshot.Classes || !snapshot.Features || !snapshot.Table) {
    return std::nullopt;
  }
  return Ground(region, snapshot, coarseness);
}

Ground::Ground(const Tile &region, const Snapshot &snapshot, Detail coarseness)
    : Region_(region),
      Patch_(snapshot.Patch),
      Classes_(snapshot.Classes),
      Features_(snapshot.Features),
      Table_(snapshot.Table),
      Coarseness_(coarseness) {
  region.AnchorEcef(0.0, AnchorEcef_);
}

Cover Ground::CoverAt(EastNorth at) const noexcept {
  const EastNorth on = Classes_->Frame().Project(Region_.Geo(at));
  double edgeM = 0.0;
  int runnerUp = -1;
  const int row = Classes_->Evaluate(on.EastM, on.NorthM, &edgeM, &runnerUp);
  if (row < 0) { return Cover::None(); }
  if (edgeM >= ClassStructure::kNoEdgeM) {
    return Cover::Of({.Row = row, .RunnerUpRow = runnerUp});
  }
  return Cover::Of({.Row = row, .RunnerUpRow = runnerUp}, static_cast<float>(edgeM));
}

} // namespace outshine::Generators
