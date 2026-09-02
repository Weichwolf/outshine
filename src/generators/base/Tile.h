#ifndef OUTSHINE_GENERATORS_BASE_TILE_H
#define OUTSHINE_GENERATORS_BASE_TILE_H

#include "Earth.h"
#include "math/Vec3.h"
#include <cstdint>

namespace outshine::Generators {

class Tile {
public:
  Tile(int zoom, int x, int y);

  [[nodiscard]] int Zoom() const { return Zoom_; }

  [[nodiscard]] int X() const { return X_; }

  [[nodiscard]] int Y() const { return Y_; }

  [[nodiscard]] bool Is(const Tile &other) const {
    return Zoom_ == other.Zoom_ && X_ == other.X_ && Y_ == other.Y_;
  }

  [[nodiscard]] uint64_t Seed() const { return Seed_; }

  [[nodiscard]] uint64_t Seed(uint64_t stream) const;

  [[nodiscard]] double AnchorLat() const { return AnchorLat_; }

  [[nodiscard]] double AnchorLon() const { return AnchorLon_; }

  [[nodiscard]] double SpanEm() const { return SpanEm_; }

  [[nodiscard]] double SpanNm() const { return SpanNm_; }

  [[nodiscard]] EastNorth Enu(LongitudeLatitude at) const;
  [[nodiscard]] LongitudeLatitude Geo(EastNorth at) const;

  [[nodiscard]] bool Holds(EastNorth at) const {
    return at.EastM >= 0.0 && at.NorthM >= 0.0 && at.EastM < SpanEm_ && at.NorthM < SpanNm_;
  }

  void AnchorEcef(double aslM, Vec3 &out) const;

  static Tile Of(int zoom, double lat, double lon);

private:
  int Zoom_, X_, Y_;
  uint64_t Seed_;
  double AnchorLat_, AnchorLon_, SpanEm_, SpanNm_;
};

} // namespace outshine::Generators
#endif
