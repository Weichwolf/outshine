#ifndef OUTSHINE_WORLD_GROUND_TILES_TILEGEODESY_H
#define OUTSHINE_WORLD_GROUND_TILES_TILEGEODESY_H

#include <cstdint>
#include <optional>

#include "Address.h"
#include "Earth.h"
#include "math/Units.h"
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

  [[nodiscard]] std::optional<Data::TileId> Tile() const {
    if (Where_ != State::Inside) { return std::nullopt; }
    return Held_;
  }

private:
  TileIndex(State where, Data::TileId held) : Where_(where), Held_(held) {}

  State Where_;
  Data::TileId Held_;
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

GeoBounds TileBounds(Data::TileId of);

Geo TileFracToGeo(TileFrac at, int z);

Ecef GeoToEcefWgs84(Geo g);

Geo EcefToGeoWgs84(Ecef p);

struct MetresPerDegree {
  double Longitude = 0.0;
  double Latitude = 0.0;
};

class EnuFrame {
public:
  enum class State { Usable, OriginTooPolar };

  static EnuFrame At(Geo origin);

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] std::optional<Enu> FromGeo(Geo g) const {
    if (Where_ != State::Usable) { return std::nullopt; }
    return Enu{.EastM = (g.LongitudeDeg - Origin_.LongitudeDeg) * Per_.Longitude,
               .NorthM = (g.LatitudeDeg - Origin_.LatitudeDeg) * Per_.Latitude,
               .UpM = g.HeightM};
  }

private:
  EnuFrame(State where, LongitudeLatitude origin, MetresPerDegree per)
      : Where_(where), Origin_(origin), Per_(per) {}

  State Where_;
  LongitudeLatitude Origin_;
  MetresPerDegree Per_;
};

class TileEnuMap {
public:
  static TileEnuMap Over(const EnuFrame &frame, Data::TileId of, uint32_t extent);

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
