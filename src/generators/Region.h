#ifndef OUTSHINE_GENERATORS_REGION_H
#define OUTSHINE_GENERATORS_REGION_H

#include <cstdint>

namespace outshine::Generators {

class Region {
public:
  Region(int zoom, int x, int y);

  [[nodiscard]] int Zoom() const { return Zoom_; }
  [[nodiscard]] int X() const { return X_; }
  [[nodiscard]] int Y() const { return Y_; }
  [[nodiscard]] bool Is(const Region &other) const {
    return Zoom_ == other.Zoom_ && X_ == other.X_ && Y_ == other.Y_;
  }

  [[nodiscard]] uint64_t Seed() const { return Seed_; }

  [[nodiscard]] uint64_t Seed(uint64_t stream) const;

  [[nodiscard]] double AnchorLat() const { return AnchorLat_; }
  [[nodiscard]] double AnchorLon() const { return AnchorLon_; }
  [[nodiscard]] double SpanEm() const { return SpanEm_; }
  [[nodiscard]] double SpanNm() const { return SpanNm_; }

  void Enu(double lat, double lon, double *eastM, double *northM) const;
  void Geo(double eastM, double northM, double *lat, double *lon) const;
  [[nodiscard]] bool Holds(double eastM, double northM) const {
    return eastM >= 0.0 && northM >= 0.0 && eastM < SpanEm_ && northM < SpanNm_;
  }

  void AnchorEcef(double aslM, double out[3]) const;

  static Region Of(int zoom, double lat, double lon);

private:
  int Zoom_, X_, Y_;
  uint64_t Seed_;
  double AnchorLat_, AnchorLon_, SpanEm_, SpanNm_;
};

}
#endif
