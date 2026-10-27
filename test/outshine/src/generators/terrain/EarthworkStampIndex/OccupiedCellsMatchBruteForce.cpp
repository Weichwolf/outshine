#include "Check.h"
#include "EarthworkStampIndex.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <print>
#include <ratio>
#include <span>
#include <vector>

namespace {
void Verify(bool sparse, bool reverse) {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t count = 1000;
  const double spacingM = sparse ? 10000 : 4;
  std::vector<EarthworkStamp> stamps(count);
  std::vector<double> widthsM(count);
  std::vector<EastNorth> points;
  for (size_t at = 0; at < count; ++at) {
    auto &stamp = stamps[at];
    const size_t row = at / 40;
    stamp.LowE = static_cast<double>(at % 40) * spacingM - 200000;
    stamp.LowN = static_cast<double>(row) * spacingM - 120000;
    stamp.HighE = stamp.LowE + 8;
    stamp.HighN = stamp.LowN + 6;
    stamp.PlanarSupportM = at % 7 == 0 ? 6 : 0;
    widthsM[at] = static_cast<double>(at % 3) * 8;
    points.push_back({.EastM = stamp.LowE, .NorthM = stamp.LowN});
    points.push_back({.EastM = stamp.HighE, .NorthM = stamp.HighN});
    points.push_back({.EastM = stamp.LowE + 4, .NorthM = stamp.LowN + 3});
  }
  points.push_back({.EastM = 900000, .NorthM = 900000});
  if (reverse) {
    std::ranges::reverse(stamps);
    std::ranges::reverse(widthsM);
  }
  const auto began = std::chrono::steady_clock::now();
  const EarthworkStampIndex index(stamps, widthsM);
  const auto built = std::chrono::steady_clock::now();
  size_t visits = 0;
  for (const auto point : points) { visits += index.At(point).size(); }
  const auto queried = std::chrono::steady_clock::now();
  for (const auto point : points) {
    const double east = std::floor(point.EastM / 32);
    const double north = std::floor(point.NorthM / 32);
    std::vector<uint32_t> expected;
    for (size_t at = 0; at < stamps.size(); ++at) {
      const auto &stamp = stamps[at];
      const double reachM = widthsM[at] + stamp.PlanarSupportM.value_or(0.0);
      if (std::floor((stamp.LowE - reachM) / 32) <= east &&
          east <= std::floor((stamp.HighE + reachM) / 32) &&
          std::floor((stamp.LowN - reachM) / 32) <= north &&
          north <= std::floor((stamp.HighN + reachM) / 32)) {
        expected.push_back(static_cast<uint32_t>(at));
      }
    }
    CHECK(std::ranges::equal(index.At(point), expected),
          "every occupied-cell query preserves all candidates in their original order");
  }
  const double buildMs = std::chrono::duration<double, std::milli>(built - began).count();
  const double queryMs = std::chrono::duration<double, std::milli>(queried - built).count();
  std::println("EARTHWORK_INDEX sparse={} reverse={} stamps={} queries={} candidates={} "
               "retained={}B build={}ms query={}ms",
               sparse,
               reverse,
               count,
               points.size(),
               visits,
               index.HeapBytes(),
               buildMs,
               queryMs);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const EarthworkStampIndex empty({}, {});
  CHECK(empty.At({}).empty() && empty.HeapBytes() == 0, "empty input retains no cell payload");
  for (const bool sparse : {false, true}) {
    for (const bool reverse : {false, true}) { Verify(sparse, reverse); }
  }
  return Report();
}
