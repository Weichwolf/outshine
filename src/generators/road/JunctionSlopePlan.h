#ifndef OUTSHINE_GENERATORS_ROAD_JUNCTIONSLOPEPLAN_H
#define OUTSHINE_GENERATORS_ROAD_JUNCTIONSLOPEPLAN_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
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

enum class JunctionSlopeRefusal { NoRoom, Deadline, ProjectionBound };

struct JunctionSlopeFailure {
  JunctionSlopeRefusal Kind;
  std::optional<size_t> Constraint;
  double MaximumRiseM = 0.0;

  [[nodiscard]] std::string_view Reason() const noexcept;
};

[[nodiscard]] std::expected<void, JunctionSlopeFailure>
FitJunctionSlopes(std::span<JunctionSlope> slopes,
                  std::span<const JunctionSlopeConstraint> constraints,
                  std::chrono::steady_clock::time_point deadline);

}
#endif
