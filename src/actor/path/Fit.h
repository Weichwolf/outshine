#ifndef OUTSHINE_ACTOR_PATH_FIT_H
#define OUTSHINE_ACTOR_PATH_FIT_H

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "ReferenceLine.h"

namespace outshine {

[[nodiscard]] double CornerRadiusM(double turnRad, double shorterLegM, double withinM);

[[nodiscard]] std::vector<double> Simplify(std::span<const double> eastNorthM, double withinM);

[[nodiscard]] std::vector<double> Simplify(std::span<const double> eastNorthM, double withinM,
                                          std::vector<size_t> &kept);

struct Fitted {
  bool Laid = false;
  size_t Vertices = 0;
  size_t Corners = 0;
  size_t Straights = 0;
  double LengthM = 0.0;
  double WorstOffsetM = 0.0;
  double WorstOffsetAtM = 0.0;
  double TightestRadiusM = 0.0;
  size_t TightestAtVertex = 0;
  double SharpestTurnRad = 0.0;
  double SharpestTurnAtM = 0.0;
  size_t TurnsPastRightAngle = 0;
  size_t TurnsPastHalfCircle = 0;
  size_t Corrected = 0;
  size_t Strained = 0;
  double StrainedWorstM = 0.0;
  double WorstVertex = 0.0;
  double WorstLegInM = 0.0;
  double WorstLegOutM = 0.0;
  double WorstTurnRad = 0.0;
  double WorstRadiusM = 0.0;
  double WorstStationM = 0.0;
  double WorstExpectedM = 0.0;
  double DriftM = 0.0;
  double DriftPerCornerM = 0.0;
  size_t Passes = 0;
  size_t Undrivable = 0;
  double UndrivableAtM = 0.0;
  size_t UnderClass = 0;
  size_t UnderClassAtVertex = 0;
  double UnderClassRadiusM = 0.0;
  double UnderClassMinimumM = 0.0;
  std::string Error;
};

[[nodiscard]] Fitted Fit(std::span<const double> eastNorthM, double withinM,
                         double tightestM, ReferenceLine &into);

[[nodiscard]] Fitted Fit(std::span<const double> eastNorthM, double withinM, double tightestM,
                         std::span<const double> classTightestM, ReferenceLine &into);

}

#endif
