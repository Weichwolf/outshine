#ifndef OUTSHINE_GENERATORS_PATH_ALIGNMENT_H
#define OUTSHINE_GENERATORS_PATH_ALIGNMENT_H

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ReferenceLine.h"

namespace outshine {

struct Refusal {
  std::string Said;
  double DemandedM = 0.0;
  size_t AtVertex = 0;
  size_t Undrivable = 0;
};

struct Bend {
  size_t FirstVertex = 0;
  size_t LastVertex = 0;
  double TurnRad = 0.0;
  double RadiusM = 0.0;
  double PiEastM = 0.0;
  double PiNorthM = 0.0;
  double TangentM = 0.0;
  double SpiralM = 0.0;
  double ArcM = 0.0;
  double AwayM = 0.0;
  double AwayShare = 0.0;
  double IntoEastM = 0.0;
  double IntoNorthM = 0.0;
  double OutOfEastM = 0.0;
  double OutOfNorthM = 0.0;
  double IntoHeadingRad = 0.0;
  double OutOfHeadingRad = 0.0;
};

struct Aligned {
  std::vector<Bend> Bends;
  size_t Runs = 0;
  size_t LongestRunVertices = 0;
  size_t SplitByAccuracy = 0;
  double TightestRadiusM = 0.0;
  double WorstAwayM = 0.0;
};

struct Laid {
  size_t Straights = 0;
  double LengthM = 0.0;
};

struct Junction {
  double HalfAM = 0.0;
  double HalfBM = 0.0;
  double DeflectionRad = 0.0;
  double ShorterLegM = 0.0;
};

[[nodiscard]] double JunctionKerbM(Junction of);

[[nodiscard]] std::expected<Aligned, Refusal> Align(std::span<const double> eastNorthM,
                                                    double withinM,
                                                    double tightestM,
                                                    std::span<const double> withinAtM = {});

[[nodiscard]] std::expected<Laid, Refusal>
LayAligned(std::span<const double> eastNorthM, const Aligned &aligned, ReferenceLine &into);

} // namespace outshine

#endif
