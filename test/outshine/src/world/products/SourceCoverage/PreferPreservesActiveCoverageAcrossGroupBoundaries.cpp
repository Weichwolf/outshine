#include "Check.h"
#include "SourceCoverage.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr uint64_t universe = 1'000'000'000'000ULL;
  using Range = SourceCoverage::Range;
  const auto coverage = [](std::initializer_list<Range> intervals) {
    return SourceCoverage::FromRanges("region", universe, intervals);
  };
  const auto expected = coverage({{10, 4}, {universe - 2, 2}});
  const auto west = coverage({{10, 2}});
  const auto east = coverage({{12, 2}});
  const auto crossing = coverage({{10, 1}, {12, 1}});
  const auto remainder = coverage({{11, 1}, {13, 1}});
  const auto distant = coverage({{universe - 2, 2}});
  CHECK(expected && west && east && crossing && remainder && distant, "test coverage is valid");
  if (!expected || !west || !east || !crossing || !remainder || !distant) { return Report(); }
  const auto fallback = std::array{&*west, &*east};
  const auto partial =
      SourceCoverage::Prefer(*expected, fallback, std::array{&*crossing, &*distant});
  CHECK(partial && partial->Retained == std::vector<size_t>({0, 1}) &&
            partial->Preferred == std::vector<size_t>({1}),
        "one crossing group cannot retire either of two resident parents");
  const auto ready =
      SourceCoverage::Prefer(*expected, fallback, std::array{&*crossing, &*remainder, &*distant});
  CHECK(ready && ready->Retained.empty() && ready->Preferred == std::vector<size_t>({0, 1, 2}),
        "the complete crossing cut replaces both parents together");
  const auto reversed =
      SourceCoverage::Prefer(*expected, std::array{&*crossing, &*remainder}, std::array{&*west});
  CHECK(reversed && reversed->Retained == std::vector<size_t>({0, 1}) &&
            reversed->Preferred.empty(),
        "a partially ready finer cut leaves its grouped predecessors resident");
  const auto invalid = SourceCoverage::Prefer(*expected, fallback, std::array{&*crossing, &*west});
  CHECK(!invalid && invalid.error() == SourceCoverageError::Overlap,
        "a tier with duplicate source ownership cannot activate");
  return Report();
}
