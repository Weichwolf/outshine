#include "HeightBandInternal.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <stop_token>
#include <vector>

namespace outshine::HeightBands {
namespace {
Checked<void> Face(const HeightBandInput &input, size_t at) {
  const auto face = input.Faces[at];
  for (const auto vertex : face) {
    if (vertex >= input.Points.size()) { return std::unexpected(Error::InvalidInput); }
  }
  const auto &a = input.Points[face[0]];
  const auto &b = input.Points[face[1]];
  const auto &c = input.Points[face[2]];
  const double area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
  if (!std::isfinite(area) || area <= 0) { return std::unexpected(Error::InvalidInput); }
  return {};
}

Checked<void> Ranges(const HeightBandInput &input, const std::stop_token &stop) {
  std::vector<uint32_t> breaks(input.Levels.size());
  for (size_t at = 0; at < input.Levels.size(); ++at) {
    if (!std::isfinite(input.Levels[at])) { return std::unexpected(Error::InvalidInput); }
    if (at != 0) {
      breaks[at] = breaks[at - 1] + static_cast<uint32_t>(input.Levels[at] <= input.Levels[at - 1]);
    }
  }
  for (size_t at = 0; at < input.Faces.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (auto valid = Face(input, at); !valid) { return valid; }
    const auto [first, count] = input.LevelRanges[at];
    if (count == 0) { continue; }
    if (first > input.Levels.size() || count > input.Levels.size() - first) {
      return std::unexpected(Error::InvalidInput);
    }
    if (count > 1 && breaks[first + count - 1] != breaks[first]) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  return {};
}
}

Checked<void>
Validate(const HeightBandInput &input, HeightBandBudget budget, const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (input.Points.size() > budget.InputPoints || input.Points.size() > budget.Points ||
      input.Faces.size() > budget.InputFaces || input.Levels.size() > budget.Levels ||
      input.Faces.size() > std::numeric_limits<uint32_t>::max() / 3 ||
      input.Points.size() >= std::numeric_limits<uint32_t>::max() ||
      input.Levels.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (input.LevelRanges.size() != input.Faces.size()) {
    return std::unexpected(Error::InvalidInput);
  }
  for (const auto &point : input.Points) {
    if (!std::ranges::all_of(point, [](double value) { return std::isfinite(value); })) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  return Ranges(input, stop);
}
}
