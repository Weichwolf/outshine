#ifndef OUTSHINE_WORLD_GROUND_TILES_TILEGEODESY_H
#define OUTSHINE_WORLD_GROUND_TILES_TILEGEODESY_H

#include <cstdint>

#include "Earth.h"
#include "Units.h"
#include "TileMath.h"
#include "Wgs84.h"

namespace outshine::Ground {

using Geo = outshine::LongitudeLatitudeHeight;

using Enu = outshine::EastNorthUp;

struct Ecef {
  double X = 0.0, Y = 0.0, Z = 0.0;
};

struct GeoBounds {
  double MinLonDeg = 0.0, MinLatDeg = 0.0, MaxLonDeg = 0.0, MaxLatDeg = 0.0;
};

struct TileFrac {
  double X = 0.0, Y = 0.0;
};

using Data::kMercatorGirthM;
using Data::kWgs84A;
using Data::kWgs84F;

class TileIndex {
public:
  enum class State { Inside, OutsideMercatorBand };

  static TileIndex Of(Geo g, int z);

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] bool TryXy(uint32_t *x, uint32_t *y) const {
    if (Where_ != State::Inside) { return false; }
    *x = X_;
    *y = Y_;
    return true;
  }

private:
  TileIndex(State where, uint32_t x, uint32_t y) : Where_(where), X_(x), Y_(y) {}

  State Where_;
  uint32_t X_, Y_;
};

inline TileFrac ToTileFracClamped(Geo g, int z) {
  const double n = std::ldexp(1.0, z);
  const double latDeg = ClampD(g.LatitudeDeg, -kMercatorLatMaxDeg, kMercatorLatMaxDeg);
  const double lonDeg = ClampD(g.LongitudeDeg, -kDegPerHalfTurn, kDegPerHalfTurn);
  const double lr = latDeg * kDeg2Rad;
  TileFrac f;
  f.X = (lonDeg + kDegPerHalfTurn) / kDegPerTurn * n;
  f.Y = (1.0 - std::log(std::tan(lr) + 1.0 / std::cos(lr)) / kPi) / 2.0 * n;
  return f;
}

[[nodiscard]] inline bool WrapTile(int z, long *x, const long *y) {
  const long n = static_cast<long>(std::ldexp(1.0, z));
  *x = ((*x % n) + n) % n;
  return *y >= 0 && *y < n;
}

GeoBounds TileBounds(int z, uint32_t x, uint32_t y);

Geo TileLocalToGeo(int z, uint32_t x, uint32_t y, uint32_t extent, int32_t localX, int32_t localY);

Geo TileFracToGeo(int z, uint32_t x, uint32_t y, double fx, double fy);

Ecef GeoToEcefWgs84(Geo g);

Geo EcefToGeoWgs84(Ecef p);

class EnuFrame {
public:
  enum class State { Usable, OriginTooPolar };

  static EnuFrame At(double originLatDeg, double originLonDeg);

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] bool TryFromGeo(Geo g, Enu *out) const {
    if (Where_ != State::Usable) { return false; }
    out->EastM = (g.LongitudeDeg - OriginLonDeg_) * MetresPerDegLon_;
    out->NorthM = (g.LatitudeDeg - OriginLatDeg_) * MetresPerDegLat_;
    out->UpM = g.HeightM;
    return true;
  }

  [[nodiscard]] bool TryToGeo(Enu e, Geo *out) const {
    if (Where_ != State::Usable) { return false; }
    out->LongitudeDeg = OriginLonDeg_ + e.EastM / MetresPerDegLon_;
    out->LatitudeDeg = OriginLatDeg_ + e.NorthM / MetresPerDegLat_;
    out->HeightM = e.UpM;
    return true;
  }

private:
  EnuFrame(State where, double latDeg, double lonDeg, double mPerDegLat, double mPerDegLon)
      : Where_(where),
        OriginLatDeg_(latDeg),
        OriginLonDeg_(lonDeg),
        MetresPerDegLat_(mPerDegLat),
        MetresPerDegLon_(mPerDegLon) {}

  State Where_;
  double OriginLatDeg_, OriginLonDeg_;
  double MetresPerDegLat_, MetresPerDegLon_;
};

class TileEnuMap {
public:
  static TileEnuMap Over(const EnuFrame &frame, int z, uint32_t x, uint32_t y, uint32_t extent);

  [[nodiscard]] double OriginE() const { return OriginE_; }

  [[nodiscard]] double OriginN() const { return OriginN_; }

  [[nodiscard]] double ScaleE() const { return ScaleE_; }

  [[nodiscard]] double ScaleN() const { return ScaleN_; }

  [[nodiscard]] uint32_t Extent() const { return Extent_; }

  [[nodiscard]] Enu Apply(int32_t localX, int32_t localY) const {
    Enu r;
    r.EastM = OriginE_ + static_cast<double>(localX) * ScaleE_;
    r.NorthM = OriginN_ + static_cast<double>(localY) * ScaleN_;
    r.UpM = 0.0;
    return r;
  }

private:
  double OriginE_ = 0.0, OriginN_ = 0.0;
  double ScaleE_ = 0.0, ScaleN_ = 0.0;
  uint32_t Extent_ = 0;
};

} // namespace outshine::Ground
#endif
