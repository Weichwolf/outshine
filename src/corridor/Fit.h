#ifndef OUTSHINE_CORRIDOR_FIT_H
#define OUTSHINE_CORRIDOR_FIT_H

#include <cstddef>
#include <string>
#include <vector>

#include "ReferenceLine.h"

namespace outshine {

[[nodiscard]] double CornerRadiusM(double turnRad, double shorterLegM, double withinM);

[[nodiscard]] std::vector<double> Simplify(const std::vector<double> &eastNorthM, double withinM);


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
  double SharpestTurnAtM = 0.0;
  size_t TurnsPastRightAngle = 0;
  size_t TurnsPastHalfCircle = 0;
  size_t Corrected = 0;
  size_t Strained = 0;
  double StrainedWorstM = 0.0;
  size_t Passes = 0;
  size_t Undrivable = 0;
  double UndrivableAtM = 0.0;
  std::string Error;
};

[[nodiscard]] Fitted Fit(const std::vector<double> &eastNorthM, double withinM,
                         double tightestM, ReferenceLine &into);

} // namespace outshine

#endif
