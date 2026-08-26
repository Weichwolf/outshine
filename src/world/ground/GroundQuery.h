#ifndef OUTSHINE_WORLD_GROUND_GROUNDQUERY_H
#define OUTSHINE_WORLD_GROUND_GROUNDQUERY_H

#include "GroundSample.h"

namespace outshine {

class GroundQuery {
public:
  virtual ~GroundQuery() = default;

  [[nodiscard]] virtual GroundSample At(double lat, double lon) const = 0;
  [[nodiscard]] virtual GroundSample Resident(double lat, double lon) const = 0;
  [[nodiscard]] virtual double PostM(double latDeg) const = 0;
};

}
#endif
