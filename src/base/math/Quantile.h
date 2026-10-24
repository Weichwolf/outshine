#ifndef OUTSHINE_MATH_QUANTILE_H
#define OUTSHINE_MATH_QUANTILE_H

#include <cmath>
#include <cstddef>
#include <expected>
#include <span>

namespace outshine {

constexpr double kMiddleQuantile = 0.50;

constexpr double kBroadQuantile = 0.95;

constexpr double kWidestQuantile = 0.99;

constexpr double kNarrowQuantile = 1.0 - kBroadQuantile;

constexpr double kNearestQuantile = 1.0 - kWidestQuantile;

enum class QuantileError { EmptySample, NonFiniteShare };

[[nodiscard]] constexpr std::expected<double, QuantileError>
QuantileOf(std::span<const double> sorted, double share) noexcept {
  if (!std::isfinite(share)) { return std::unexpected(QuantileError::NonFiniteShare); }
  if (sorted.empty()) { return std::unexpected(QuantileError::EmptySample); }
  if (share <= 0.0) { return sorted.front(); }
  if (share >= 1.0) { return sorted.back(); }
  const double at = static_cast<double>(sorted.size()) * share;
  auto rank = static_cast<size_t>(at);
  if (static_cast<double>(rank) < at) { ++rank; }
  return sorted[rank > 0 ? rank - 1 : 0];
}

}

#endif
