#include "TimestampRange.h"
#include "Check.h"
#include <cstdint>
#include <limits>

int main() {
  constexpr double kShortHostNs = 20.0;
  constexpr double kAmbiguousHostNs = 300.0;
  constexpr double kFullWrapHostNs = 12.0;
  constexpr double kInsideFractionalHostNs = 26.0;
  constexpr double kOutsideFractionalHostNs = 24.0;
  constexpr double kMetadataHostNs = 10.0;

  using outshine::Render::Native::Vulkan::TimestampIntervalFits;
  CHECK(TimestampIntervalFits(250, 4, 8, 1.0, kShortHostNs),
        "one eight-bit wrap gives ten elapsed ticks within a proven twenty-nanosecond bound");
  CHECK(!TimestampIntervalFits(250, 4, 8, 1.0, kAmbiguousHostNs),
        "the same raw pair is ambiguous when multiple wraps fit the host interval");
  CHECK(
      TimestampIntervalFits(std::numeric_limits<uint64_t>::max() - 4, 5, 64, 1.0, kFullWrapHostNs),
      "full-width unsigned counter subtraction preserves a single wrap without shifting by 64");
  CHECK(!TimestampIntervalFits(100, 2, 64, 1.0, kShortHostNs),
        "a backwards reset outside the possible host interval is unavailable");
  CHECK(TimestampIntervalFits(100, 150, 64, 0.5, kInsideFractionalHostNs),
        "fractional tick periods fit the independently bounded physical duration");
  CHECK(!TimestampIntervalFits(100, 150, 64, 0.5, kOutsideFractionalHostNs),
        "a counter jump longer than the entire host interval is disjoint");
  CHECK(!TimestampIntervalFits(1, 2, 0, 1.0, kMetadataHostNs) &&
            !TimestampIntervalFits(1, 2, 65, 1.0, kMetadataHostNs),
        "unsupported precision never enters undefined mask arithmetic");
  CHECK(!TimestampIntervalFits(1, 2, 64, 0.0, kMetadataHostNs) &&
            !TimestampIntervalFits(1, 2, 64, -1.0, kMetadataHostNs) &&
            !TimestampIntervalFits(
                1, 2, 64, std::numeric_limits<double>::infinity(), kMetadataHostNs),
        "invalid periods cannot manufacture elapsed time");
  CHECK(!TimestampIntervalFits(1, 2, 64, 1.0, -1.0) &&
            !TimestampIntervalFits(1, 2, 64, 1.0, std::numeric_limits<double>::infinity()),
        "an invalid host interval is no evidence for a counter span");
  return outshine::Test::Report();
}
