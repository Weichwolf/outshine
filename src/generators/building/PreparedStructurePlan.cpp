#include "PreparedStructurePlan.h"
#include "BuildingScratch.h"
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
  plan.Facade = layout.Facade;
  plan.Street = standing.Street;
  plan.AnchorEcef = anchor;
  return plan;
}

bool KnownEmptyPreparedStructure(const PreparedStructureTile &base, size_t source) {
  if (source >= base.Structures.size() || source >= base.Surfaces.size() ||
      base.PointsLatLon.size() % 2 != 0) {
    return false;
  }
  const auto &surface = base.Surfaces[source];
  if (surface.FaceCount() != 0 || surface.PreparedSelection()) { return false; }
  const auto &prepared = base.Structures[source];
  const auto &layout = prepared.Layout;
  const size_t points = base.PointsLatLon.size() / 2;
  if (layout.PointCount < 3 || layout.LocalFirst > points ||
      layout.PointCount > points - layout.LocalFirst || layout.FirstHole > base.Holes.size() ||
      layout.HoleCount > base.Holes.size() - layout.FirstHole ||
      prepared.CornerFirst > base.CornerAslM.size() ||
      layout.PointCount > base.CornerAslM.size() - prepared.CornerFirst) {
    return false;
  }
  const auto plan = PreparedStructurePlan(
      prepared,
      base.PointsLatLon,
      base.Holes,
      std::span(base.CornerAslM).subspan(prepared.CornerFirst, layout.PointCount),
      base.AnchorEcef);
  BuildingScratch scratch;
  const auto rebuilt = BuildingSurface::Prepare(plan, scratch);
  return !rebuilt && rebuilt.error() == StructureMeshError::UnsupportedFootprint;
}
}
