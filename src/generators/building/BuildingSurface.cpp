#include "BuildingSurface.h"
#include "BuildingWallNormals.h"
#include "BuildingMaterials.h"
#include "BuildingPreparation.h"
#include "FacadeUv.h"
#include "RoofSurface.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <vector>
#include <optional>
#include <span>
#include <utility>

namespace outshine::Generators {
namespace {

Vec3 Local(const EnuAxes &axes, const Vec3 &value) {
  return {{Dot(value, axes.East), Dot(value, axes.North), Dot(value, axes.Up)}};
}

Vec3 Global(const EnuAxes &axes, const Vec3 &value) {
  return axes.East * value[0] + axes.North * value[1] + axes.Up * value[2];
}

double RoofDeck(const BuildingShape &shape) {
  return shape.SeatM + shape.FootM + shape.EavesM +
         (shape.Roof == RoofKind::Flat ? shape.RiseM : 0.0);
}

double Cross(const EastNorth &a, const EastNorth &b) {
  return a.EastM * b.NorthM - a.NorthM * b.EastM;
}

void Walls(const BuildingShape &shape,
           std::span<const EastNorth> ring,
           bool hole,
           double bottom,
           const Vec3 &origin,
           const Vec3 &direction,
           double minimum,
           double &maximum,
           size_t part,
           size_t &face,
           std::optional<BuildingSurface::Hit> &hit) {
  double area = 0.0;
  for (size_t at = 0; at < ring.size(); ++at) {
    area += Cross(ring[at], ring[(at + 1) % ring.size()]);
  }
  const double orientation = (area >= 0.0) != hole ? 1.0 : -1.0;
  const EastNorth ray{.EastM = direction[0], .NorthM = direction[1]};
  const RoofSurface roof(shape);
  for (size_t at = 0; at < ring.size(); ++at, ++face) {
    const auto &a = ring[at];
    const auto &b = ring[(at + 1) % ring.size()];
    const EastNorth edge{.EastM = b.EastM - a.EastM, .NorthM = b.NorthM - a.NorthM};
    const double divisor = Cross(ray, edge);
    if (divisor == 0.0) { continue; }
    const EastNorth offset{.EastM = a.EastM - origin[0], .NorthM = a.NorthM - origin[1]};
    const double along = Cross(offset, edge) / divisor;
    const double fraction = Cross(offset, ray) / divisor;
    if (along < minimum || along > maximum || fraction < 0.0 || fraction > 1.0 ||
        !std::isfinite(along)) {
      continue;
    }
    const EastNorth point{.EastM = a.EastM + fraction * edge.EastM,
                          .NorthM = a.NorthM + fraction * edge.NorthM};
    const double z = origin[2] + along * direction[2];
    const double top = RoofDeck(shape) + std::max(roof.HeightAt(point), 0.0);
    if (z < bottom || z > top) { continue; }
    const double length = std::hypot(edge.EastM, edge.NorthM);
    hit = BuildingSurface::Hit{
        .Along = along,
        .Normal = {{orientation * edge.NorthM / length, -orientation * edge.EastM / length, 0.0}},
        .Part = part,
        .Face = face};
    maximum = along;
  }
}

std::optional<BuildingSurface::Hit> TracePart(const BuildingShape &shape,
                                              double minimumHeightM,
                                              const Vec3 &origin,
                                              const Vec3 &direction,
                                              double minimum,
                                              double maximum,
                                              size_t part,
                                              std::vector<double> &cuts) {
  std::optional<BuildingSurface::Hit> hit;
  const RoofSurface roof(shape);
  const double bottom = BuildingBottomM(shape, minimumHeightM);
  if (direction[2] != 0.0) {
    const double along = (bottom - origin[2]) / direction[2];
    if (along >= minimum && along <= maximum &&
        roof.Contains({.EastM = origin[0] + along * direction[0],
                       .NorthM = origin[1] + along * direction[1]})) {
      hit = BuildingSurface::Hit{.Along = along, .Normal = {{0, 0, -1}}, .Part = part, .Face = 0};
      maximum = along;
    }
  }
  const auto roofHit =
      roof.Trace(origin - Vec3{{0, 0, RoofDeck(shape)}}, direction, minimum, maximum, cuts);
  if (roofHit) {
    hit = BuildingSurface::Hit{
        .Along = roofHit->Along, .Normal = roofHit->Normal, .Part = part, .Face = 1};
    maximum = roofHit->Along;
  }
  size_t face = 2;
  Walls(shape, shape.Ring, false, bottom, origin, direction, minimum, maximum, part, face, hit);
  for (const auto &hole : shape.Holes) {
    Walls(shape, hole, true, bottom, origin, direction, minimum, maximum, part, face, hit);
  }
  if (hit && hit->Face >= 2) {
    hit->Gable = origin[2] + hit->Along * direction[2] > RoofDeck(shape);
  }
  return hit;
}

struct Edge {
  const EastNorth *A, *B;
  bool Entrance = false, Party = false;
};

Edge EdgeOf(const BuildingShape &shape, size_t face) {
  size_t at = face - 2;
  if (at < shape.Ring.size()) {
    return {.A = &shape.Ring[at],
            .B = &shape.Ring[(at + 1) % shape.Ring.size()],
            .Entrance = std::cmp_equal(at, shape.FrontEdge),
            .Party = shape.PartyWallEdges[at] != 0};
  }
  at -= shape.Ring.size();
  for (const auto &hole : shape.Holes) {
    if (at < hole.size()) { return {.A = &hole[at], .B = &hole[(at + 1) % hole.size()]}; }
    at -= hole.size();
  }
  return {.A = nullptr, .B = nullptr};
}

Vec2f TextureAt(const BuildingShape &shape, size_t face, const Vec3 &point, bool gable) {
  if (face < 2) {
    Facade material = shape.Roof == RoofKind::Flat ? Facade::RoofFlat : Facade::RoofPitch;
    if (face == 0) { material = Facade::Plinth; }
    return {{FaceUvX(material, shape.Ident), static_cast<float>(point[2])}};
  }
  const auto edge = EdgeOf(shape, face);
  if (edge.A == nullptr || edge.B == nullptr) { return {}; }
  const double e = edge.B->EastM - edge.A->EastM;
  const double n = edge.B->NorthM - edge.A->NorthM;
  const double length = std::hypot(e, n);
  const double fraction =
      length > 0.0
          ? ((point[0] - edge.A->EastM) * e + (point[1] - edge.A->NorthM) * n) / (length * length)
          : 0.0;
  const double bays = gable || edge.Party ? 0.0 : FacadeBays(length, shape.BayM);
  return {{FacadeUvX(shape.OpeningStyle,
                     edge.Entrance && !gable ? Fields::Entrance : Fields::Back,
                     shape.WallVariant,
                     static_cast<float>(bays * fraction)),
           FacadeUvY(static_cast<float>((point[2] - shape.SeatM - shape.FootM) / shape.FloorM))}};
}

}

std::expected<BuildingSurface, StructureMeshError>
BuildingSurface::Prepare(const StructurePlan &plan, BuildingScratch &scratch) {
  const auto shapes = PrepareBuildingShapes(plan, scratch);
  if (!shapes) { return std::unexpected(shapes.error()); }
  if (shapes->empty()) { return std::unexpected(StructureMeshError::UnsupportedFootprint); }
  BuildingSurface source;
  const LongitudeLatitudeHeight origin{.LongitudeDeg = plan.RingLatLon[1],
                                       .LatitudeDeg = plan.RingLatLon[0],
                                       .HeightM = plan.BaseAslM};
  GeoToEcef(origin, source.Origin_);
  source.Origin_ = source.Origin_ - plan.AnchorEcef;
  source.Axes_ =
      EnuAxesEcef({.LongitudeDeg = origin.LongitudeDeg, .LatitudeDeg = origin.LatitudeDeg});
  source.MinimumHeightM_ = plan.MinimumHeightM;
  source.WallColour_ = plan.WallColour;
  source.Shapes_.assign(shapes->begin(), shapes->end());
  source.FaceOffsets_.push_back(0);
  for (const auto &shape : source.Shapes_) {
    size_t count = shape.Ring.size() + 2;
    for (const auto &hole : shape.Holes) { count += hole.size(); }
    source.FaceOffsets_.push_back(source.FaceOffsets_.back() + count);
    const double bottom = BuildingBottomM(shape, plan.MinimumHeightM);
    for (const auto &point : shape.Ring) {
      for (const double height : {bottom, shape.TopM()}) {
        source.Bounds_.Cover(source.Origin_ +
                             Global(source.Axes_, {{point.EastM, point.NorthM, height}}));
      }
    }
  }
  return source;
}

BuildingSurface::Face BuildingSurface::FaceAt(size_t index) const noexcept {
  const auto &offsets = Resident().FaceOffsets_;
  const auto found = std::ranges::upper_bound(offsets, index);
  const auto part = static_cast<size_t>(found - offsets.begin() - 1);
  return {.Part = part, .Side = index - offsets[part]};
}

bool BuildingSurface::SupportsProjection() const noexcept {
  if (Selection_) { return Selection_->Projectable; }
  return !Shapes_.empty() && std::ranges::all_of(Shapes_, [](const BuildingShape &shape) {
    return shape.RiseM == 0.0 || (shape.Roof != RoofKind::Dome && shape.Roof != RoofKind::Sawtooth);
  });
}

std::optional<BuildingSurface::Hit> BuildingSurface::Trace(const Ray &ray,
                                                           double minimum,
                                                           double maximum,
                                                           std::vector<double> &cuts) const {
  const Vec3 localOrigin = Local(Axes_, ray.Origin - Origin_);
  const Vec3 localDirection = Local(Axes_, ray.Direction);
  const auto &shapes = Resident().Shapes_;
  std::optional<Hit> nearest;
  for (size_t part = 0; part < shapes.size(); ++part) {
    auto hit = TracePart(
        shapes[part], MinimumHeightM_, localOrigin, localDirection, minimum, maximum, part, cuts);
    if (hit) {
      maximum = hit->Along;
      hit->Normal = Global(Axes_, hit->Normal);
      nearest = hit;
    }
  }
  return nearest;
}

StoredVertex BuildingSurface::VertexAt(const Hit &hit, const Vec3 &position) const noexcept {
  const auto &shape = Resident().Shapes_[hit.Part];
  const Vec3 local = Local(Axes_, position - Origin_);
  const Vec2f uv = TextureAt(shape, hit.Face, local, hit.Gable);
  Vec3 normal = hit.Normal;
  if (!hit.Gable && hit.Face >= 2 && hit.Face - 2 < shape.Ring.size() &&
      HasCurvedShaftWalls(shape)) {
    normal = Global(Axes_,
                    BuildingWallInterpolatedNormal(shape,
                                                   hit.Face - 2,
                                                   {.EastM = local[0], .NorthM = local[1]},
                                                   Local(Axes_, hit.Normal)));
  }
  return StoredVertex::Of({{static_cast<float>(position[0]),
                            static_cast<float>(position[1]),
                            static_cast<float>(position[2])}},
                          uv,
                          {{static_cast<float>(normal[0]),
                            static_cast<float>(normal[1]),
                            static_cast<float>(normal[2])}});
}

Vec3f BuildingSurface::ColourFactor(const Hit &hit) const noexcept {
  if (!WallColour_ || IsRoof(hit)) { return {{1, 1, 1}}; }
  return {{(*WallColour_)[0] / kBuildingWallColour[0],
           (*WallColour_)[1] / kBuildingWallColour[1],
           (*WallColour_)[2] / kBuildingWallColour[2]}};
}

}
