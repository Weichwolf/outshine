#include "SpeciesMixture.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array<std::string_view, 4> names{"b", "a", "zero", "c"};
  constexpr std::array<double, 4> weights{1, 1, 0, 2};
  const auto mixture = SpeciesMixture::Prepare(names, weights);
  CHECK(mixture.has_value(), "finite declared relative weights prepare once");
  if (!mixture) { return Report(); }
  CHECK(mixture->Choose(0) == 1 && mixture->Choose(std::nextafter(0.25, 0)) == 1 &&
            mixture->Choose(0.25) == 0 && mixture->Choose(0.5) == 3 &&
            mixture->Choose(std::nextafter(1.0, 0)) == 3,
        "half-open CDF intervals retain canonical name order and original catalogue indices");
  constexpr uint32_t draws = 10000;
  std::array<uint32_t, names.size()> counts{};
  for (uint32_t at = 0; at < draws; ++at) {
    const auto chosen = mixture->Choose((static_cast<double>(at) + 0.5) / draws);
    CHECK(chosen && *chosen < counts.size(), "supported mixtures always name a held species");
    if (chosen && *chosen < counts.size()) { ++counts[*chosen]; }
  }
  CHECK((counts == std::array<uint32_t, 4>{2500, 2500, 0, 5000}),
        "stratified draws exactly follow the declared distribution and exclude zero weight");
  constexpr std::array<std::string_view, 3> scaledNames{"a", "b", "c"};
  const auto huge = SpeciesMixture::Prepare(scaledNames, std::array{1e308, 1e308, 0.0});
  const auto small = SpeciesMixture::Prepare(scaledNames, std::array{1.0, 1.0, 0.0});
  CHECK(huge && small, "finite weights near the double limit normalize without sum overflow");
  if (huge && small) {
    for (const auto draw : {0.0, 0.49, 0.5, std::nextafter(1.0, 0)}) {
      CHECK(huge->Choose(draw) == small->Choose(draw), "common scaling preserves the choice");
    }
  }
  const auto zero = SpeciesMixture::Prepare(names, std::array{0.0, 0.0, 0.0, 0.0});
  CHECK(zero && !zero->Choose(0) && !zero->Choose(std::nextafter(1.0, 0)),
        "explicit zero support never becomes uniform catalogue selection");
  const auto negative = SpeciesMixture::Prepare(names, std::array{1.0, -1.0, 0.0, 2.0});
  const auto duplicate =
      SpeciesMixture::Prepare(std::array<std::string_view, 2>{"a", "a"}, std::array{1.0, 1.0});
  const auto absent = SpeciesMixture::Prepare(names, std::span<const double>{});
  CHECK(!negative && !duplicate && !absent, "malformed declarations fail before sampling");
  for (const auto bad :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    CHECK(!SpeciesMixture::Prepare(scaledNames, std::array{1.0, bad, 0.0}),
          "nonfinite source weights cannot enter a prepared distribution");
  }
  return Report();
}
