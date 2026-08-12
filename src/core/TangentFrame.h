/* THE PLANE A SCENE'S METRES ARE MEASURED IN: one ECEF tangent plane, fixed at its anchor. It
 * travels with whatever is measured in it, so a set of metres and the plane they mean cannot be
 * handed over separately and drift apart. */
#ifndef TANGENTFRAME_H
#define TANGENTFRAME_H

#include <cmath>

#include "Geodesy.h"
#include "Units.h"

namespace outshine {

class TangentFrame {
public:
  TangentFrame() : TangentFrame(0.0, 0.0) {}
  static TangentFrame At(double latDeg, double lonDeg) { return TangentFrame(latDeg, lonDeg); }

  double AnchorLat() const { return Lat_; }
  double AnchorLon() const { return Lon_; }
  const double *OriginEcef() const { return O_; }
  const double *EastEcef() const { return East_; }
  const double *NorthEcef() const { return North_; }

  /* Exact, through the ellipsoid — the axes a fragment projects its own camera-relative offset on
   * are these, so processor and device place a geodetic point identically by construction. */
  void Project(double latDeg, double lonDeg, double *eastM, double *northM) const {
    double p[3];
    GeoToEcef(latDeg, lonDeg, 0.0, p);
    const double d[3] = {p[0] - O_[0], p[1] - O_[1], p[2] - O_[2]};
    *eastM = d[0] * East_[0] + d[1] * East_[1] + d[2] * East_[2];
    *northM = d[0] * North_[0] + d[1] * North_[1] + d[2] * North_[2];
  }

  /* The planar inverse, not the exact one: over the 900 m a scatter reaches its error stays under a
   * metre, against a height posting of 47 m. */
  void Geo(double eastM, double northM, double *latDeg, double *lonDeg) const {
    *latDeg = Lat_ + northM / kMPerDeg;
    *lonDeg = Lon_ + eastM / (kMPerDeg * std::cos(Lat_ * kDeg2Rad));
  }

private:
  TangentFrame(double latDeg, double lonDeg) : Lat_(latDeg), Lon_(lonDeg) {
    GeoToEcef(latDeg, lonDeg, 0.0, O_);
    double up[3];
    EnuAxesEcef(latDeg, lonDeg, East_, North_, up);
  }

  double Lat_, Lon_;
  double O_[3], East_[3], North_[3];
};

} // namespace outshine
#endif
