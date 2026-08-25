#ifndef OUTSHINE_ACTOR_PATH_ALIGNMENT_H
#define OUTSHINE_ACTOR_PATH_ALIGNMENT_H

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

struct Bend {
  size_t FirstVertex = 0;
  size_t LastVertex = 0;
  double TurnRad = 0.0;
  double RadiusM = 0.0;
  double PiEastM = 0.0;
  double PiNorthM = 0.0;
  double TangentM = 0.0;
  double AwayM = 0.0;
};

struct Aligned {
  std::vector<Bend> Bends;
  size_t Runs = 0;
  size_t LongestRunVertices = 0;
  double TightestRadiusM = 0.0;
  double WorstAwayM = 0.0;
};

[[nodiscard]] std::expected<Aligned, std::string> Align(std::span<const double> eastNorthM,
                                                        double withinM, double tightestM);

}

#endif
