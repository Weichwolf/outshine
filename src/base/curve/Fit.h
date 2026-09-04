#ifndef OUTSHINE_BASE_CURVE_FIT_H
#define OUTSHINE_BASE_CURVE_FIT_H

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "ReferenceLine.h"

namespace outshine {

[[nodiscard]] std::vector<double> Simplify(std::span<const double> eastNorthM, double withinM);

[[nodiscard]] std::vector<double>
Simplify(std::span<const double> eastNorthM, double withinM, std::vector<size_t> &kept);

struct Fitted {
  bool Laid = false;
  size_t Vertices = 0;
  size_t Corners = 0;
  size_t Straights = 0;
  double LengthM = 0.0;
  double TightestRadiusM = 0.0;
  double TightestDemandedM = 0.0;
  size_t TightestDemandedAtVertex = 0;
  size_t Passes = 0;
  size_t Undrivable = 0;
  double UndrivableAtM = 0.0;
  size_t Runs = 0;
  std::string Error;
};

[[nodiscard]] Fitted Fit(std::span<const double> eastNorthM,
                         double withinM,
                         double tightestM,
                         ReferenceLine &into,
                         std::span<const double> withinAtM = {});

} // namespace outshine

#endif
