#ifndef OUTSHINE_MATH_QUANTILE_H
#define OUTSHINE_MATH_QUANTILE_H

#include <cmath>
#include <cstddef>
#include <expected>
#include <span>

/// @file
/// Nearest-rank selection for sorted measurements; no interpolation.
namespace outshine {

/// Median rank, expressed as a fraction of the sample count.
constexpr double kMiddleQuantile = 0.50;
/// 95th percentile rank, expressed as a fraction of the sample count.
constexpr double kBroadQuantile = 0.95;
/// 99th percentile rank, expressed as a fraction of the sample count.
constexpr double kWidestQuantile = 0.99;
/// Lower-tail rank complementary to kBroadQuantile.
constexpr double kNarrowQuantile = 1.0 - kBroadQuantile;
/// Lower-tail rank complementary to kWidestQuantile.
constexpr double kNearestQuantile = 1.0 - kWidestQuantile;

/// Reasons why a quantile cannot be selected.
enum class QuantileError {
  EmptySample,   ///< No observation exists; zero would invent a measurement.
  NonFiniteShare ///< NaN or infinity cannot identify a rank.
};

/// Selects the observation at one-based rank ceil(n * share), without interpolation.
/// @param sorted Borrowed observations in ascending order, containing no NaNs.
///               Ordered infinities and duplicate values are allowed. The caller
///               sorts and validates once before querying ranks; this function does
///               not scan the sample. Storage must remain readable and unchanged
///               during the call; no reference to it is retained.
/// @param share Dimensionless rank fraction. Finite values outside [0, 1] clamp to
///              the first or last observation. Non-finite values are rejected.
/// @return A copied observation in the input units, or QuantileError. Invalid share
///         takes precedence over empty sample. Empty samples never produce a value.
/// @pre sorted is ascending and contains no NaNs; violating this precondition does
///      not produce a meaningful quantile. Concurrent calls may share immutable data.
/// @note O(1) time, no allocation, no exceptions. Rank multiplication uses double
///       arithmetic; values at a rank boundary follow its floating-point rounding.
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
