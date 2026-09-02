#ifndef OUTSHINE_WORLD_GROUND_GROUNDQUERY_H
#define OUTSHINE_WORLD_GROUND_GROUNDQUERY_H

#include "Earth.h"
#include "GroundSample.h"

namespace outshine::Ground {
class GroundBlock;
}

namespace outshine {

namespace Ground {

struct TileSpot {
  int Zoom = 0;
  long X = 0;
  long Y = 0;
};

} // namespace Ground

class GroundQuery {
public:
  virtual ~GroundQuery() = default;

  [[nodiscard]] virtual GroundSample At(LongitudeLatitude at) const = 0;
  [[nodiscard]] virtual GroundSample Resident(LongitudeLatitude at) const = 0;
  [[nodiscard]] virtual Ground::GroundBlock BlockAt(Ground::TileSpot at) const = 0;
  [[nodiscard]] virtual double PostM(double latDeg) const = 0;

  [[nodiscard]] virtual int BlockZoom() const { return 0; }
};

} // namespace outshine
#endif
