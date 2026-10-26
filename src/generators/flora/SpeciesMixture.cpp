#include "SpeciesMixture.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators {

std::expected<SpeciesMixture, std::string>
SpeciesMixture::Prepare(std::span<const std::string_view> names, std::span<const double> weights) {
  if (names.empty() || names.size() > kMostSpecies || weights.size() != names.size()) {
    return std::unexpected("species mixture requires 1..64 names and one weight per name");
  }
  std::vector<size_t> order(names.size());
  std::ranges::iota(order, size_t{0});
  std::ranges::sort(order, [&](size_t a, size_t b) { return names[a] < names[b]; });
  double maximum = 0;
  for (size_t at = 0; at < order.size(); ++at) {
    const auto index = order[at];
    if (names[index].empty() || (at > 0 && names[index] == names[order[at - 1]]) ||
        !std::isfinite(weights[index]) || weights[index] < 0) {
      return std::unexpected(
          "species mixture requires unique names and finite nonnegative weights");
    }
    maximum = std::max(maximum, weights[index]);
  }
  SpeciesMixture result;
  if (maximum == 0) { return result; }
  double total = 0;
  result.Cumulative_.reserve(static_cast<size_t>(
      std::ranges::count_if(weights, [](double weight) { return weight > 0; })));
  for (const auto index : order) {
    if (weights[index] == 0) { continue; }
    total += weights[index] / maximum;
    result.Cumulative_.push_back({.Upper = total, .Species = static_cast<uint32_t>(index)});
  }
  for (auto &entry : result.Cumulative_) { entry.Upper /= total; }
  return result;
}

std::optional<uint32_t> SpeciesMixture::Choose(double uniform) const noexcept {
  assert(std::isfinite(uniform) && uniform >= 0 && uniform < 1);
  const auto found = std::ranges::upper_bound(Cumulative_, uniform, {}, &Entry::Upper);
  return found == Cumulative_.end() ? std::nullopt : std::optional(found->Species);
}

}
