#include "Ground.h"

namespace outshine::Generators {

std::optional<Ground> Ground::Of(const Region &region, const Snapshot &snapshot) {
  if (!snapshot.Patch || !snapshot.Classes || !snapshot.Features || !snapshot.Table)
    return std::nullopt;
  return Ground(region, snapshot);
}

Ground::Ground(const Region &region, const Snapshot &snapshot)
    : Region_(region), Patch_(snapshot.Patch), Classes_(snapshot.Classes),
      Features_(snapshot.Features), Table_(snapshot.Table) {
  region.AnchorEcef(0.0, AnchorEcef_);
}

Cover Ground::CoverAt(double eastM, double northM) const noexcept {

  double lat = 0.0, lon = 0.0;
  Region_.Geo(eastM, northM, &lat, &lon);
  double e = 0.0, n = 0.0;
  Classes_->Frame().Project(lat, lon, &e, &n);
  double edgeM = 0.0;
  int runnerUp = -1;
  const int row = Classes_->Evaluate(e, n, &edgeM, &runnerUp);
  if (row < 0) return Cover::None();
  if (edgeM >= ClassStructure::kNoEdgeM) return Cover::Of(row, runnerUp);
  return Cover::Of(row, (float)edgeM, runnerUp);
}

}
