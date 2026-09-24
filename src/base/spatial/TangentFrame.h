#ifndef OUTSHINE_BASE_SPATIAL_TANGENTFRAME_H
#define OUTSHINE_BASE_SPATIAL_TANGENTFRAME_H

#include <cmath>

#include "Geodesy.h"
#include "math/Units.h"
#include "math/Vec3.h"

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

  [[nodiscard]] EastNorthUp ToLocalPosition(const Vec3 &ecef) const {
    return ToLocalDirection(ecef - O_);
  }

  [[nodiscard]] EastNorthUp ToLocalPosition(const LongitudeLatitudeHeight &at) const {
    Vec3 p;
    GeoToEcef(at, p);
    return ToLocalPosition(p);
  }

  [[nodiscard]] EastNorthUp ToLocalDirection(const Vec3 &ecef) const {
    return {.EastM = ecef[0] * East_[0] + ecef[1] * East_[1] + ecef[2] * East_[2],
            .NorthM = ecef[0] * North_[0] + ecef[1] * North_[1] + ecef[2] * North_[2],
            .UpM = ecef[0] * Up_[0] + ecef[1] * Up_[1] + ecef[2] * Up_[2]};
  }

  [[nodiscard]] EastNorth ToLocalGroundPosition(LongitudeLatitude at) const {
    const EastNorthUp on = ToLocalPosition(
        {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = 0.0});
    return {.EastM = on.EastM, .NorthM = on.NorthM};
  }

  [[nodiscard]] LongitudeLatitude ApproximateGeographicAt(EastNorth at) const {
    return {.LongitudeDeg = Anchor_.LongitudeDeg +
                            at.EastM / (kMPerDegLon * std::cos(Anchor_.LatitudeDeg * kDeg2Rad)),
            .LatitudeDeg = Anchor_.LatitudeDeg + at.NorthM / kMPerDegLat};
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

}

#endif
