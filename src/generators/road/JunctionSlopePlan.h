#ifndef OUTSHINE_GENERATORS_ROAD_JUNCTIONSLOPEPLAN_H
#define OUTSHINE_GENERATORS_ROAD_JUNCTIONSLOPEPLAN_H

#include <chrono>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {

struct JunctionSlope {
  double East = 0.0;
  double North = 0.0;
  double Maximum = 0.0;
};

struct JunctionSlopeTerm {
  uint32_t Junction = 0;
  double EastM = 0.0;
  double NorthM = 0.0;
};

struct JunctionSlopeConstraint {
  std::vector<JunctionSlopeTerm> Terms;
  double MinimumRiseM = 0.0;
};

[[nodiscard]] std::expected<void, std::string_view>
FitJunctionSlopes(std::span<JunctionSlope> slopes,
                  std::span<const JunctionSlopeConstraint> constraints,
                  std::chrono::steady_clock::time_point deadline);

}
#endif
