#ifndef OUTSHINE_CONTENT_SHADE_TANGENTFRAME_H
#define OUTSHINE_CONTENT_SHADE_TANGENTFRAME_H

#include <cmath>

#include "math/Vec3.h"
#include "Geodesy.h"
#include "Units.h"

namespace outshine {

class TangentFrame {
public:
  TangentFrame() : TangentFrame(0.0, 0.0) {}

  static TangentFrame At(double latDeg, double lonDeg) { return {latDeg, lonDeg}; }

  [[nodiscard]] double AnchorLat() const { return Lat_; }

  [[nodiscard]] double AnchorLon() const { return Lon_; }

  [[nodiscard]] const Vec3 &OriginEcef() const { return O_; }

  [[nodiscard]] const Vec3 &EastEcef() const { return East_; }

  [[nodiscard]] const Vec3 &NorthEcef() const { return North_; }

  [[nodiscard]] const Vec3 &UpEcef() const { return Up_; }

  void Place(const Vec3 &ecef, double *eastM, double *upM, double *northM) const {
    const Vec3 d = ecef - O_;
    *eastM = d[0] * East_[0] + d[1] * East_[1] + d[2] * East_[2];
    *upM = d[0] * Up_[0] + d[1] * Up_[1] + d[2] * Up_[2];
    *northM = d[0] * North_[0] + d[1] * North_[1] + d[2] * North_[2];
  }

  void Place(
      double latDeg, double lonDeg, double altM, double *eastM, double *upM, double *northM) const {
    Vec3 p;
    GeoToEcef(latDeg, lonDeg, altM, p);
    Place(p, eastM, upM, northM);
  }

  void Turn(const Vec3 &ecef, double *eastward, double *upward, double *northward) const {
    *eastward = ecef[0] * East_[0] + ecef[1] * East_[1] + ecef[2] * East_[2];
    *upward = ecef[0] * Up_[0] + ecef[1] * Up_[1] + ecef[2] * Up_[2];
    *northward = ecef[0] * North_[0] + ecef[1] * North_[1] + ecef[2] * North_[2];
  }

  void Project(double latDeg, double lonDeg, double *eastM, double *northM) const {
    Vec3 p;
    GeoToEcef(latDeg, lonDeg, 0.0, p);
    const Vec3 d = p - O_;
    *eastM = d[0] * East_[0] + d[1] * East_[1] + d[2] * East_[2];
    *northM = d[0] * North_[0] + d[1] * North_[1] + d[2] * North_[2];
  }

  void Geo(double eastM, double northM, double *latDeg, double *lonDeg) const {
    *latDeg = Lat_ + northM / kMPerDeg;
    *lonDeg = Lon_ + eastM / (kMPerDeg * std::cos(Lat_ * kDeg2Rad));
  }

private:
  TangentFrame(double latDeg, double lonDeg) : Lat_(latDeg), Lon_(lonDeg) {
    GeoToEcef(latDeg, lonDeg, 0.0, O_);
    EnuAxesEcef(latDeg, lonDeg, East_, North_, Up_);
  }

  double Lat_, Lon_;
  Vec3 O_, East_, North_, Up_;
};

} // namespace outshine
#endif
