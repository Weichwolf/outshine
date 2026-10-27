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
enum class SupportSize : uint8_t { Small, Mixed };

void Verify(bool sparse, bool reverse, SupportSize size = SupportSize::Small) {
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
    const double edgeM =
        size == SupportSize::Mixed ? static_cast<double>(size_t{8} << (at % 12)) : 8;
    stamp.HighE = stamp.LowE + edgeM;
    stamp.HighN = stamp.LowN + edgeM * 0.75;
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
  EarthworkStampQuery scratch;
  size_t visits = 0;
  for (const auto point : points) { visits += index.At(point, scratch).size(); }
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
    CHECK(std::ranges::equal(index.At(point, scratch), expected),
          "every occupied-cell query preserves all candidates in their original order");
  }
  const double buildMs = std::chrono::duration<double, std::milli>(built - began).count();
  const double queryMs = std::chrono::duration<double, std::milli>(queried - built).count();
  CHECK(index.HeapBytes() + scratch.HeapBytes() <= count * size_t{2048},
        "retained index and query payload stays within two KiB per test support");
  std::println("EARTHWORK_INDEX sparse={} reverse={} stamps={} queries={} candidates={} "
               "retained={}B build={}ms query={}ms mixed={}",
               sparse,
               reverse,
               count,
               points.size(),
               visits,
               index.HeapBytes(),
               buildMs,
               queryMs,
               size == SupportSize::Mixed);
}

void VerifyLargeSupports() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto began = std::chrono::steady_clock::now();
  const EarthworkStampIndex index = [] {
    std::array<EarthworkStamp, 3> stamps;
    stamps[0].HighE = stamps[0].HighN = 70000;
    stamps[1].LowE = stamps[1].LowN = -100000;
    stamps[1].HighE = stamps[1].HighN = -30000;
    stamps[2].HighE = stamps[2].HighN = 4;
    const std::array<double, 3> widths{};
    return EarthworkStampIndex(stamps, widths);
  }();
  const double buildMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  EarthworkStampQuery first;
  EarthworkStampQuery second;
  const auto near = index.At({.EastM = 1, .NorthM = 1}, first);
  CHECK(std::ranges::equal(near, std::array<uint32_t, 2>{0, 2}),
        "large and small supports retain the original order after input storage expires");
  CHECK(std::ranges::equal(index.At({.EastM = -99999, .NorthM = -99999}, second),
                           std::array<uint32_t, 1>{1}) &&
            std::ranges::equal(near, std::array<uint32_t, 2>{0, 2}),
        "independent caller scratch preserves simultaneous index readers");
  CHECK(index.At({.EastM = 70016, .NorthM = 1}, first).empty() &&
            index.At({.EastM = -29984, .NorthM = -30000}, first).empty(),
        "coarse-cell matches outside the original fine bounds are discarded");
  CHECK(std::ranges::equal(index.At({.EastM = 70000, .NorthM = 70000}, first),
                           std::array<uint32_t, 1>{0}),
        "the original fine-cell upper boundary remains covered");
  CHECK(index.HeapBytes() + first.HeapBytes() + second.HeapBytes() <= size_t{3} * 2048,
        "three bounded supports fit the explicit six-KiB retained-index allowance");
  std::println("EARTHWORK_INDEX large=70000m supports=3 retained={}B scratch={}B build={}ms",
               index.HeapBytes(),
               first.HeapBytes() + second.HeapBytes(),
               buildMs);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const EarthworkStampIndex empty({}, {});
  EarthworkStampQuery scratch;
  CHECK(empty.At({}, scratch).empty() && empty.HeapBytes() == 0,
        "empty input retains no cell payload");
  for (const bool sparse : {false, true}) {
    for (const bool reverse : {false, true}) { Verify(sparse, reverse); }
  }
  Verify(false, false, SupportSize::Mixed);
  Verify(true, true, SupportSize::Mixed);
  VerifyLargeSupports();
  return Report();
}
