#include "PreparedStructurePlan.h"
#include <span>
#include <cstddef>

namespace outshine::Generators {

StructurePlan PreparedStructurePlan(const PreparedStructure &prepared,
                                    std::span<const double> points,
                                    std::span<const GeographicRing> holes,
                                    std::span<const double> corners,
                                    const Vec3 &anchor) {
  const auto &layout = prepared.Layout;
  const auto &standing = prepared.Standing;
  StructurePlan plan;
  plan.InnerRings = holes.subspan(layout.FirstHole, layout.HoleCount);
  plan.RingPointsLatLon = points;
  plan.RingLatLon = points.subspan(static_cast<size_t>(layout.LocalFirst) * 2,
                                   static_cast<size_t>(layout.PointCount) * 2);
  plan.BaseAslM = standing.BaseM;
  plan.SeatAslM = standing.SeatM;
  plan.FootAslM = standing.FootM;
  plan.CornerAslM = corners;
  plan.HeightM = layout.MinimumHeightM != 0.0 ? layout.HeightM : standing.HeightM;
  plan.MinimumHeightM = layout.MinimumHeightM;
  plan.HeightMeasured = standing.Source == Ground::BuildingHeightSource::Declared;
  plan.PitchedShare = static_cast<double>(layout.Pitched);
  plan.WallColour = layout.WallColour;
  plan.Street = standing.Street;
  plan.AnchorEcef = anchor;
  return plan;
}
}
