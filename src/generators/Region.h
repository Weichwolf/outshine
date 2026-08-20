#ifndef REGION_H
#define REGION_H

#include <cstdint>

namespace outshine::Generators {

class Region {
public:
  Region(int zoom, int x, int y);

  int Zoom() const { return Zoom_; }
  int X() const { return X_; }
  int Y() const { return Y_; }
  [[nodiscard]] bool Is(const Region &other) const {
    return Zoom_ == other.Zoom_ && X_ == other.X_ && Y_ == other.Y_;
  }

  uint64_t Seed() const { return Seed_; }

  uint64_t Seed(uint64_t stream) const;

  double AnchorLat() const { return AnchorLat_; }
  double AnchorLon() const { return AnchorLon_; }
  double SpanEm() const { return SpanEm_; }
  double SpanNm() const { return SpanNm_; }

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
