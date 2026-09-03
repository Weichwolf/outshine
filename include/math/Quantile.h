#ifndef OUTSHINE_MATH_QUANTILE_H
#define OUTSHINE_MATH_QUANTILE_H

#include <array>
#include <cstddef>
#include <span>

/// @file
/// What a quantile IS here, and the four ranks this engine reports.
///
/// A frame time is never a mean -- a mean hides the one frame that stuttered behind the hundred
/// that did not. So every population this engine publishes is reported at ranks, and a rank needs
/// a DEFINITION before it means anything: three stood in this tree at once, one rounding, one
/// truncating and one over histogram bins, so `p99` named two different numbers depending on who
/// asked. This is the one definition, and it is the standard's rather than ours.
namespace outshine {

/// @name The ranks this engine reports
/// Named because a bare `0.95` in a call says nothing about what it is a share OF.
/// @{

/// The middle of a population -- half of it is at or below this.
constexpr double kMiddleQuantile = 0.50;

/// The broad rank: only one sample in twenty is worse.
constexpr double kBroadQuantile = 0.95;

/// The widest rank this engine reports. Beyond it a population of 120 holds one sample, and one
/// sample is an anecdote rather than a measurement.
constexpr double kWidestQuantile = 0.99;

/// The narrow rank -- the broad one read from the other end, for a population whose GOOD tail is
/// the interesting one.
constexpr double kNarrowQuantile = 1.0 - kBroadQuantile;

/// The near rank, for a population read from its FAST end -- a speed the plan holds rather than a
/// time a frame took.
constexpr double kNearestQuantile = 1.0 - kWidestQuantile;

/// @}

/// The value of a SORTED sample at a share of it -- the NEAREST-RANK quantile.
///
/// NIST's definition: the smallest value at or below which at least `share` of the sample lies,
/// which is the element at one-based rank `ceil(n * share)`. No interpolation happens, so the
/// answer is always a value the population actually held -- which is what a reader of a frame time
/// wants, because a p99 of 16.8 ms should name a frame that took 16.8 ms.
///
/// @param sorted A sample in ascending order. Sorting is the CALLER's, because a caller usually
///               reads several ranks off one sample and sorting it once is the point.
/// @param share  Where in the sample to read, 0 to 1. Outside that it clamps to the ends.
/// @return The value at that rank, or 0 for an empty sample.
[[nodiscard]] constexpr double QuantileOf(std::span<const double> sorted, double share) {
  if (sorted.empty()) { return 0.0; }
  if (share <= 0.0) { return sorted.front(); }
  if (share >= 1.0) { return sorted.back(); }
  const double at = static_cast<double>(sorted.size()) * share;
  auto rank = static_cast<size_t>(at);
  if (static_cast<double>(rank) < at) { ++rank; }
  return sorted[rank > 0 ? rank - 1 : 0];
}

namespace Held {
/// A five-value sample the asserts below interrogate.
inline constexpr std::array<double, 5> kFive = {1.0, 2.0, 3.0, 4.0, 5.0};

/// The share of that sample ONE of its values is -- the width of a rank step.
inline constexpr double kOneStep = 1.0 / static_cast<double>(kFive.size());
} // namespace Held

static_assert(QuantileOf({}, kMiddleQuantile) == 0.0, "an empty sample has no rank to read");
static_assert(QuantileOf(Held::kFive, 0.0) == Held::kFive.front() &&
                  QuantileOf(Held::kFive, 1.0) == Held::kFive.back(),
              "the ends are the ends, and no rounding may walk off either");
static_assert(QuantileOf(Held::kFive, kMiddleQuantile) == Held::kFive[2],
              "five values put the middle ON a value rather than between two");
static_assert(QuantileOf(Held::kFive, Held::kOneStep) == Held::kFive.front() &&
                  QuantileOf(Held::kFive, Held::kOneStep + Held::kOneStep / 2) == Held::kFive[1],
              "the rank steps where a value is PASSED, so a share inside a step reads the value "
              "that covers it and never the one below");
static_assert(kNarrowQuantile > kNearestQuantile && kWidestQuantile > kBroadQuantile,
              "the two pairs are ordered, so a rank read from either end cannot cross the other");
static_assert(QuantileOf(Held::kFive, kWidestQuantile) == Held::kFive.back(),
              "p99 of five values is the worst of them -- a small sample cannot hide its tail");

} // namespace outshine

#endif
