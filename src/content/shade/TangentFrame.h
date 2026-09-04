#ifndef OUTSHINE_CONTENT_SHADE_TANGENTFRAME_H
#define OUTSHINE_CONTENT_SHADE_TANGENTFRAME_H

#include "ground/TileMeshes.h"

#include <vector>
#include <span>
#include <cmath>

#include "math/Vec3.h"
#include "Geodesy.h"
#include "math/Units.h"

namespace outshine {

class TangentFrame {
public:
  TangentFrame() : TangentFrame(LongitudeLatitude{}) {}

  static TangentFrame At(LongitudeLatitude anchor) { return TangentFrame(anchor); }

  [[nodiscard]] LongitudeLatitude Anchor() const { return Anchor_; }

  [[nodiscard]] const Vec3 &OriginEcef() const { return O_; }

  [[nodiscard]] const Vec3 &EastEcef() const { return East_; }

  [[nodiscard]] const Vec3 &NorthEcef() const { return North_; }

  [[nodiscard]] const Vec3 &UpEcef() const { return Up_; }

  [[nodiscard]] EastNorthUp Place(const Vec3 &ecef) const { return Turn(ecef - O_); }

  [[nodiscard]] EastNorthUp Place(const LongitudeLatitudeHeight &at) const {
    Vec3 p;
    GeoToEcef(at, p);
    return Place(p);
  }

  [[nodiscard]] EastNorthUp Turn(const Vec3 &ecef) const {
    return {.EastM = ecef[0] * East_[0] + ecef[1] * East_[1] + ecef[2] * East_[2],
            .NorthM = ecef[0] * North_[0] + ecef[1] * North_[1] + ecef[2] * North_[2],
            .UpM = ecef[0] * Up_[0] + ecef[1] * Up_[1] + ecef[2] * Up_[2]};
  }

  [[nodiscard]] EastNorth Project(LongitudeLatitude at) const {
    const EastNorthUp on =
        Place({.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = 0.0});
    return {.EastM = on.EastM, .NorthM = on.NorthM};
  }

  [[nodiscard]] LongitudeLatitude Geo(EastNorth at) const {
    return {.LongitudeDeg = Anchor_.LongitudeDeg +
                            at.EastM / (kMPerDeg * std::cos(Anchor_.LatitudeDeg * kDeg2Rad)),
            .LatitudeDeg = Anchor_.LatitudeDeg + at.NorthM / kMPerDeg};
  }

private:
  explicit TangentFrame(LongitudeLatitude anchor) : Anchor_(anchor) {
    GeoToEcef({.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg}, O_);
    const EnuAxes axes =
        EnuAxesEcef({.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg});
    East_ = axes.East;
    North_ = axes.North;
    Up_ = axes.Up;
  }

  LongitudeLatitude Anchor_;
  Vec3 O_, East_, North_, Up_;
};

struct Carrying {
  std::span<const StoredVertex> Corners;
  Vec3 AnchorEcefM = {{0.0, 0.0, 0.0}};
  size_t Already = 0;
};

struct Carried {
  std::vector<float> &PlacesM;
  std::vector<float> &Turned;
};

inline size_t CarryIntoTheFrame(const Carrying &from, const TangentFrame &standing, Carried into) {
  const std::span<const StoredVertex> corners = from.Corners;
  const Vec3 &anchor = from.AnchorEcefM;
  std::vector<float> &places = into.PlacesM;
  std::vector<float> &turned = into.Turned;
  size_t already = from.Already;
  const size_t count = corners.size();
  if (already > count || places.size() != already * 3 || turned.size() != already * 3) {
    already = 0;
  }
  places.resize(count * 3);
  turned.resize(count * 3);
  for (size_t at = already; at < count; ++at) {
    const StoredVertex &one = corners[at];
    const Vec3 held = {{anchor[0] + static_cast<double>(one.pos[0]),
                        anchor[1] + static_cast<double>(one.pos[1]),
                        anchor[2] + static_cast<double>(one.pos[2])}};
    double eastM = 0.0;
    double upM = 0.0;
    double northM = 0.0;
    const EastNorthUp eastMEnu = standing.Place(held);
    eastM = eastMEnu.EastM;
    upM = eastMEnu.UpM;
    northM = eastMEnu.NorthM;
    places[at * 3] = static_cast<float>(eastM);
    places[at * 3 + 1] = static_cast<float>(upM);
    places[at * 3 + 2] = static_cast<float>(-northM);
    const Vec3f facing = one.norm();
    const Vec3 aim = {{static_cast<double>(facing[0]),
                       static_cast<double>(facing[1]),
                       static_cast<double>(facing[2])}};
    double alongEast = 0.0;
    double alongUp = 0.0;
    double alongNorth = 0.0;
    const EastNorthUp alongEastEnu = standing.Turn(aim);
    alongEast = alongEastEnu.EastM;
    alongUp = alongEastEnu.UpM;
    alongNorth = alongEastEnu.NorthM;
    turned[at * 3] = static_cast<float>(alongEast);
    turned[at * 3 + 1] = static_cast<float>(alongUp);
    turned[at * 3 + 2] = static_cast<float>(-alongNorth);
  }
  return count;
}
} // namespace outshine
#endif
