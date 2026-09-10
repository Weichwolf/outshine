#ifndef OUTSHINE_WORLD_GROUND_GROUNDPOLLBUDGET_H
#define OUTSHINE_WORLD_GROUND_GROUNDPOLLBUDGET_H
#include <cmath>
#include <expected>
#include <limits>
#include <string_view>

namespace outshine::Ground {
inline constexpr double kGroundPollsPerSecond = 1000.0;

namespace Says {
inline constexpr std::string_view InvalidPatience =
    "ground patience must be finite, nonnegative and fit the poll-count budget";
}

[[nodiscard]] inline std::expected<int, std::string_view>
GroundPollAttempts(double seconds) noexcept {
  if (!std::isfinite(seconds) || seconds < 0.0 ||
      seconds > static_cast<double>(std::numeric_limits<int>::max()) / kGroundPollsPerSecond) {
    return std::unexpected(Says::InvalidPatience);
  }
  return static_cast<int>(seconds * kGroundPollsPerSecond);
}
}
#endif
