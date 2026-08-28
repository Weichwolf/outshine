#ifndef OUTSHINE_WORLD_GROUND_GROUNDQUERY_H
#define OUTSHINE_WORLD_GROUND_GROUNDQUERY_H

#include "GroundSample.h"

namespace outshine::Ground {
class GroundBlock;
}

namespace outshine {

class GroundQuery {
public:
  virtual ~GroundQuery() = default;

  [[nodiscard]] virtual GroundSample At(double lat, double lon) const = 0;
  [[nodiscard]] virtual GroundSample Resident(double lat, double lon) const = 0;
  [[nodiscard]] virtual Ground::GroundBlock BlockAt(int z, long x, long y) const = 0;
  [[nodiscard]] virtual double PostM(double latDeg) const = 0;

  [[nodiscard]] virtual int BlockZoom() const { return 0; }
};

}
#endif
