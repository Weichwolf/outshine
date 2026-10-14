#include "Check.h"
#include "LeafOpticsTable.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace {
using Table = outshine::Generators::LeafOpticsTable;

Table::Spectrum Optics(double u, double v) {
  const double value = .05 + .1 * u + .07 * v + .05 * u * v;
  return {value, value * .8, value * .4, value * .5, value, value * .3};
}

std::vector<Table::Spectrum> Values(size_t side) {
  std::vector<Table::Spectrum> values;
  for (size_t x = 0; x < side; ++x) {
    for (size_t y = 0; y < side; ++y) {
      values.push_back(Optics(static_cast<double>(x) / static_cast<double>(side - 1),
                              static_cast<double>(y) / static_cast<double>(side - 1)));
    }
  }
  return values;
}

void Lookup() {
  using namespace outshine::Test;
  for (size_t side : {3u, 5u, 33u, 65u}) {
    auto source = Values(side);
    auto made = Table::Create(side, source);
    CHECK(made.has_value(), "valid bounded table is available without a spectral model");
    if (!made) { continue; }
    std::ranges::fill(source, Table::Spectrum{});
    CHECK(made->Values().front()[0] > 0, "prepared table owns its immutable samples");
    for (int x = 0; x <= 20; ++x) {
      for (int y = 0; y <= 20; ++y) {
        const double u = static_cast<double>(x) / 20;
        const double v = static_cast<double>(y) / 20;
        const Table::Pigments pigments{std::expm1(u * std::log1p(80.0)),
                                       std::expm1(v * std::log1p(10.0))};
        const auto sampled =
            made->Sample({std::min(pigments[0], 80.0), std::min(pigments[1], 10.0)});
        CHECK(sampled.has_value(), "full prepared pigment domain remains available");
        if (!sampled) { continue; }
        const auto expected = Optics(u, v);
        for (size_t channel = 0; channel < expected.size(); ++channel) {
          CHECK_NEAR((*sampled)[channel],
                     expected[channel],
                     1e-13,
                     "linear R/T",
                     "independent bilinear polynomial in logarithmic pigment coordinates");
        }
      }
    }
  }
}

void Gamut() {
  using namespace outshine::Test;
  constexpr Table::Spectrum raw{.5, -.1, .1, .2, .8, .1};
  const std::vector<Table::Spectrum> values(9, raw);
  const auto table = Table::Create(3, values);
  CHECK(table.has_value(), "finite raw colour outside display gamut keeps valid optical energy");
  if (!table) { return; }
  const auto sampled = table->Sample({45, .2});
  CHECK(sampled.has_value(), "valid raw optics resolve to the bounded display response");
  if (!sampled) { return; }
  for (size_t sheet = 0; sheet < 2; ++sheet) {
    const size_t at = sheet * 3;
    const long double original = raw[at] * .2126L + raw[at + 1] * .7152L + raw[at + 2] * .0722L;
    const long double actual =
        (*sampled)[at] * .2126L + (*sampled)[at + 1] * .7152L + (*sampled)[at + 2] * .0722L;
    CHECK_NEAR(actual,
               original,
               1e-13,
               "linear luminance",
               "joint gamut reduction preserves each sheet luminance");
  }
  const double maximum = std::max({(*sampled)[0], (*sampled)[1], (*sampled)[2]}) +
                         std::max({(*sampled)[3], (*sampled)[4], (*sampled)[5]});
  CHECK(maximum <= 1 + 1e-14 && std::ranges::all_of(*sampled, [](double x) { return x >= 0; }),
        "joint R/T reduction remains nonnegative and energy bounded");
  CHECK_NEAR(
      (*sampled)[1], 0, 1e-13, "linear R/T", "the nearest admissible gamut boundary is reached");
}

void Boundaries() {
  using namespace outshine::Test;
  const auto values = Values(3);
  for (double limit : {std::numeric_limits<double>::denorm_min(), 1e-308, 1e308}) {
    const auto table = Table::Create(3, values, {limit, limit});
    CHECK(table.has_value(), "every finite positive limit satisfies the same domain contract");
    if (!table) { continue; }
    for (double pigment : {0.0, limit}) {
      const auto sampled = table->Sample({pigment, pigment});
      CHECK(sampled && std::ranges::all_of(*sampled, [](double x) { return std::isfinite(x); }),
            "tiny and large limits never overflow a reciprocal scale");
    }
  }
  CHECK(!Table::Create(2, values), "undersized tables reject before sampling");
  CHECK(!Table::Create(66, values), "table preparation stays within its declared storage bound");
  CHECK(!Table::Create(3, values, {0, 10}), "zero pigment limits cannot define a grid");
  const auto table = Table::Create(3, values);
  if (!table) { return; }
  for (const Table::Pigments pigments :
       {Table::Pigments{-1, 1},
        Table::Pigments{81, 1},
        Table::Pigments{1, std::numeric_limits<double>::quiet_NaN()}}) {
    CHECK(!table->Sample(pigments), "invalid pigment queries explicitly reject");
  }
}
}

int main() {
  Lookup();
  Gamut();
  Boundaries();
  return outshine::Test::Report();
}
