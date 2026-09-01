#include "Ground.h"
#include <optional>

namespace outshine::Generators {

std::optional<Ground> Ground::Of(const Tile &region, const Snapshot &snapshot) {
  if (!snapshot.Patch || !snapshot.Classes || !snapshot.Features || !snapshot.Table) {
    return std::nullopt;
  }
  return Ground(region, snapshot);
}

Ground::Ground(const Tile &region, const Snapshot &snapshot)
    : Region_(region),
      Patch_(snapshot.Patch),
      Classes_(snapshot.Classes),
      Features_(snapshot.Features),
      Table_(snapshot.Table) {
  region.AnchorEcef(0.0, AnchorEcef_);
}

Cover Ground::CoverAt(double eastM, double northM) const noexcept {
  double lat = 0.0;
  double lon = 0.0;
  Region_.Geo(eastM, northM, &lat, &lon);
  double e = 0.0;
  double n = 0.0;
  Classes_->Frame().Project(lat, lon, &e, &n);
  double edgeM = 0.0;
  int runnerUp = -1;
  const int row = Classes_->Evaluate(e, n, &edgeM, &runnerUp);
  if (row < 0) { return Cover::None(); }
  if (edgeM >= ClassStructure::kNoEdgeM) { return Cover::Of(row, runnerUp); }
  return Cover::Of(row, static_cast<float>(edgeM), runnerUp);
}

} // namespace outshine::Generators
