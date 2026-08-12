/* WHERE A TILE IS ON THE EARTH: Web Mercator slippy tiles, MVT local coordinates, the local ENU
 * tangent plane and WGS84 ECEF.
 *
 * TWO ANSWERS TO ONE QUESTION, AND THE NAME SAYS WHICH. TileIndex::Of REFUSES a latitude outside the
 * Mercator band; ToTileFracClamped folds it onto the band's edge. Both read kMercatorLatMaxDeg, so
 * they cannot disagree about where the map stops.
 *
 * The tile_enu_map fast path keeps a KNOWN residual of ~0.3 m — a property, not a defect, and not to
 * be "repaired" with trigonometry in the hot loop. */
#ifndef TILEGEODESY_H
#define TILEGEODESY_H

#include <cstdint>

#include "TileMath.h"

namespace outshine::World {

/* WGS84 lon/lat in degrees, altitude in metres above the ELLIPSOID. Terrarium heights are
 * orthometric and the geoid undulation (~85 m globally) is ignored — a slow, smooth vertical bias
 * with no visible effect. */
struct Geo {
  double LonDeg = 0.0, LatDeg = 0.0, AltM = 0.0;
};

/* Local tangent-plane ENU in metres, relative to an EnuFrame's origin. */
struct Enu {
  double E = 0.0, N = 0.0, U = 0.0;
};

/* X toward (lat 0, lon 0), Y toward (lat 0, lon 90E), Z toward the north pole. Right-handed. */
struct Ecef {
  double X = 0.0, Y = 0.0, Z = 0.0;
};

struct GeoBounds {
  double MinLonDeg = 0.0, MinLatDeg = 0.0, MaxLonDeg = 0.0, MaxLatDeg = 0.0;
};

/* Position in tile units at one zoom: the integer part is the tile, the fraction is the place inside
 * it, x east from the west edge and y south from the north edge. */
struct TileFrac {
  double X = 0.0, Y = 0.0;
};

/* WGS84 equatorial semi-major axis. */
constexpr double kEarthRadiusM = 6378137.0;
constexpr double kWgs84A = 6378137.0;
constexpr double kWgs84F = 1.0 / 298.257223563;

/* THE TILE CONTAINING A POINT, or the reason there is none. Exactly on a boundary the east/south
 * tile wins (floor semantics). The indices are reachable only through the state: a latitude past the
 * Mercator band has no tile at any zoom, and a caller that read x and y off a refusal would place
 * the world at the pole. */
class TileIndex {
 public:
  enum class State { Inside, OutsideMercatorBand };

  static TileIndex Of(Geo g, int z);

  [[nodiscard]] State Where() const { return Where_; }
  [[nodiscard]] bool TryXy(uint32_t *x, uint32_t *y) const {
    if (Where_ != State::Inside) return false;
    *x = X_;
    *y = Y_;
    return true;
  }

 private:
  TileIndex(State where, uint32_t x, uint32_t y) : Where_(where), X_(x), Y_(y) {}

  State Where_;
  uint32_t X_, Y_;
};

/* THE CLAMPING PROJECTION. A latitude outside the band comes back as the pole-most tile instead of a
 * refusal, which is what a raster sampler wants and what a placement must never have. */
inline TileFrac ToTileFracClamped(Geo g, int z) {
  const double n = std::ldexp(1.0, z);
  const double latDeg = ClampD(g.LatDeg, -kMercatorLatMaxDeg, kMercatorLatMaxDeg);
  const double lonDeg = ClampD(g.LonDeg, -180.0, 180.0);
  const double lr = latDeg * kPi / 180.0;
  TileFrac f;
  f.X = (lonDeg + 180.0) / 360.0 * n;
  f.Y = (1.0 - std::log(std::tan(lr) + 1.0 / std::cos(lr)) / kPi) / 2.0 * n;
  return f;
}

/* x wraps at the dateline, y does not: false means the row is off the map entirely. */
[[nodiscard]] inline bool WrapTile(int z, long *x, long *y) {
  const long n = (long)std::ldexp(1.0, z);
  *x = ((*x % n) + n) % n;
  return *y >= 0 && *y < n;
}

GeoBounds TileBounds(int z, uint32_t x, uint32_t y);

/* MVT local coordinate -> geographic. Geometry just outside [0..extent] is accepted (features may
 * bleed) and projected by the same linear extrapolation. */
Geo TileLocalToGeo(int z, uint32_t x, uint32_t y, uint32_t extent, int32_t localX, int32_t localY);

/* Fractional tile (fx,fy in [0..1], origin top-left) -> geographic in full double precision: the
 * canonical projector for the ECEF path, because the integer MVT lattice rounds ~0.18 m at z14 and
 * would break the sub-cm offset guarantee. */
Geo TileFracToGeo(int z, uint32_t x, uint32_t y, double fx, double fy);

Ecef GeoToEcefWgs84(Geo g);
/* Bowring's closed-form inverse, sub-mm over the whole ellipsoid. At the poles (x=y=0) lon is 0. */
Geo EcefToGeoWgs84(Ecef p);

/* THE ORIGIN-DEPENDENT FACTORS, cached so a per-vertex conversion is multiplies and adds. The
 * conversions are reachable only through the state: the ENU scale factor degenerates near the poles,
 * and a frame built there converts every vertex to nonsense without saying so. */
class EnuFrame {
 public:
  enum class State { Usable, OriginTooPolar };

  /* Usable for |origin latitude| <= 89.9 degrees. */
  static EnuFrame At(double originLatDeg, double originLonDeg);

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] bool TryFromGeo(Geo g, Enu *out) const {
    if (Where_ != State::Usable) return false;
    out->E = (g.LonDeg - OriginLonDeg_) * MetresPerDegLon_;
    out->N = (g.LatDeg - OriginLatDeg_) * MetresPerDegLat_;
    out->U = g.AltM;
    return true;
  }
  /* Exact for this model: both conversions are affine. */
  [[nodiscard]] bool TryToGeo(Enu e, Geo *out) const {
    if (Where_ != State::Usable) return false;
    out->LonDeg = OriginLonDeg_ + e.E / MetresPerDegLon_;
    out->LatDeg = OriginLatDeg_ + e.N / MetresPerDegLat_;
    out->AltM = e.U;
    return true;
  }

 private:
  EnuFrame(State where, double latDeg, double lonDeg, double mPerDegLat, double mPerDegLon)
      : Where_(where), OriginLatDeg_(latDeg), OriginLonDeg_(lonDeg),
        MetresPerDegLat_(mPerDegLat), MetresPerDegLon_(mPerDegLon) {}

  State Where_;
  double OriginLatDeg_, OriginLonDeg_;
  double MetresPerDegLat_, MetresPerDegLon_;
};

/* PER-VERTEX ENU FAST PATH: (localX, localY) -> (e, n) is affine at the scale of one tile, and the
 * lat->n residual over a z14 tile (<1.5 km) is under a metre — swamped by the tangent-plane error.
 * A map over an unusable frame carries zero scale, so it places every vertex at the origin rather
 * than somewhere plausible. */
class TileEnuMap {
 public:
  static TileEnuMap Over(const EnuFrame &frame, int z, uint32_t x, uint32_t y, uint32_t extent);

  double OriginE() const { return OriginE_; }
  double OriginN() const { return OriginN_; }
  double ScaleE() const { return ScaleE_; }   /* metres per local unit */
  double ScaleN() const { return ScaleN_; }   /* NEGATIVE: local y grows south */
  uint32_t Extent() const { return Extent_; }

  Enu Apply(int32_t localX, int32_t localY) const {
    Enu r;
    r.E = OriginE_ + (double)localX * ScaleE_;
    r.N = OriginN_ + (double)localY * ScaleN_;
    r.U = 0.0;
    return r;
  }

 private:
  double OriginE_ = 0.0, OriginN_ = 0.0;
  double ScaleE_ = 0.0, ScaleN_ = 0.0;
  uint32_t Extent_ = 0;
};

}  // namespace outshine::World
#endif
