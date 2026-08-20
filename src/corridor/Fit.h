#ifndef OUTSHINE_CORRIDOR_FIT_H
#define OUTSHINE_CORRIDOR_FIT_H

#include <cstddef>
#include <string>
#include <vector>

#include "ReferenceLine.h"

namespace outshine {

struct Fitted {
  bool Laid = false;
  size_t Vertices = 0;
  size_t Corners = 0;
  size_t Straights = 0;
  double LengthM = 0.0;
  double WorstOffsetM = 0.0;
  double WorstOffsetAtM = 0.0;
  double TightestRadiusM = 0.0;
  double SharpestTurnRad = 0.0;
  std::string Error;
};

[[nodiscard]] Fitted Fit(const std::vector<double> &eastNorthM, double withinM,
                         ReferenceLine &into);

} // namespace outshine

#endif
