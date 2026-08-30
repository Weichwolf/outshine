#ifndef OUTSHINE_WORLD_GROUND_STRUCTUREMESHER_H
#define OUTSHINE_WORLD_GROUND_STRUCTUREMESHER_H

#include <vector>

#include "Span.h"

namespace outshine {

struct WayLine {
  Span<const double> LatLon;
  double HalfWidthM = 0.0;
  double MinLat = 0.0, MinLon = 0.0, MaxLat = 0.0, MaxLon = 0.0;
};

struct Frontage {
  bool Known = false;

  double KerbEm = 0.0, KerbNm = 0.0;
  double AlongE = 0.0, AlongN = 0.0;
  double ToStreetE = 0.0, ToStreetN = 0.0;
};

struct StructurePlan {
  Span<const double> RingLatLon;
  double BaseAslM = 0.0;

  Span<const double> CornerAslM;

  double HeightM = 0.0;
  bool HeightMeasured = false;
  Frontage Street;

  const double *AnchorEcef = nullptr;

  // THE FOCAL LENGTH IN PIXELS, which is what turns a size into a number of pixels: a feature of h
  // metres at d metres covers focalPx * h / d of them. It is H / (2 tan(fov/2)) and it is the ONLY
  // thing a level-of-detail threshold can honestly be derived from -- a threshold in metres answers
  // the same for a 6 m shed and a 100 m tower, which cover 1 and 17 pixels at the same distance.
  // Zero means undeclared, and an undeclared scale keeps FULL detail and is counted rather than
  // guessed at.
  double FocalPx = 0.0;
};

class StructureMesher {
public:
  virtual ~StructureMesher() = default;
  StructureMesher(const StructureMesher &) = delete;
  StructureMesher &operator=(const StructureMesher &) = delete;

  virtual void Mesh(const StructurePlan &plan, std::vector<float> &soup) const noexcept = 0;

protected:
  StructureMesher() = default;
};

} // namespace outshine
#endif
