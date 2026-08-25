#ifndef OUTSHINE_CONTENT_SHADE_TANGENTFRAME_H
#define OUTSHINE_CONTENT_SHADE_TANGENTFRAME_H

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

  void Project(double latDeg, double lonDeg, double *eastM, double *northM) const {
    double p[3];
    GeoToEcef(latDeg, lonDeg, 0.0, p);
    const double d[3] = {p[0] - O_[0], p[1] - O_[1], p[2] - O_[2]};
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
    double up[3];
    EnuAxesEcef(latDeg, lonDeg, East_, North_, up);
  }

  double Lat_, Lon_;
  double O_[3], East_[3], North_[3];
};

}
#endif
