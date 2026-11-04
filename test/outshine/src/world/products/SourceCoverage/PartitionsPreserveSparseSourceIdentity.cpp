#include "SourceCoverage.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Range = SourceCoverage::Range;
  const auto expected = SourceCoverage::FromRanges(
      "source-a",
      1000000000000ULL,
      std::array{Range{.First = 10, .Count = 4}, Range{.First = 999999999990ULL, .Count = 10}});
  const auto first = SourceCoverage::FromRanges(
      "source-a",
      1000000000000ULL,
      std::array{Range{.First = 10, .Count = 2}, Range{.First = 999999999990ULL, .Count = 10}});
  const auto second = SourceCoverage::FromRanges(
      "source-a",
      1000000000000ULL,
      std::array{Range{.First = 12, .Count = 1}, Range{.First = 13, .Count = 1}});
  CHECK(expected && first && second, "sparse coverage does not allocate by domain size");
  if (!expected || !first || !second) { return Report(); }
  CHECK(expected->Contains(*first) && expected->Contains(*second) &&
            expected->Contains(*expected) && !first->Contains(*expected),
        "containment follows sparse intervals without enumerating their source domain");
  CHECK(SourceCoverage::Partition(*expected, std::array{&*second, &*first}),
        "unordered products can exactly partition a sparse source domain");
  CHECK(second->Ranges().size() == 1 && second->Ranges()[0] == (Range{.First = 12, .Count = 2}),
        "adjacent ranges have one canonical identity");
  CHECK(!SourceCoverage::Partition(*expected, std::array{&*first}),
        "a missing source interval never becomes complete coverage");
  const auto overlap = SourceCoverage::Partition(*expected, std::array{&*first, &*first, &*second});
  CHECK(!overlap && overlap.error() == SourceCoverageError::Overlap,
        "duplicate ownership is refused even when its union looks complete");
  const auto foreign =
      SourceCoverage::FromRanges("source-b", second->SourceCount(), second->Ranges());
  const auto otherCount =
      SourceCoverage::FromRanges("source-a", 1000000000001ULL, second->Ranges());
  CHECK(foreign && otherCount, "foreign coverage may be valid in its own domain");
  if (!foreign || !otherCount) { return Report(); }
  CHECK(!expected->Contains(*foreign) && !expected->Contains(*otherCount),
        "source containment refuses another revision or universe");
  CHECK(!SourceCoverage::Partition(*expected, std::array{&*first, &*foreign}) &&
            !SourceCoverage::Partition(*expected, std::array{&*first, &*otherCount}),
        "revision and source count must both match before handoff");
  const std::array<const SourceCoverage *, 1> missing{nullptr};
  CHECK(!SourceCoverage::Partition(*expected, missing), "missing product metadata is refused");
  CHECK(
      !SourceCoverage::FromRanges("", 10, {}) &&
          !SourceCoverage::FromRanges("source-a", 10, std::array{Range{.First = 9, .Count = 2}}) &&
          !SourceCoverage::FromRanges("source-a", 10, std::array{Range{.First = 1, .Count = 0}}),
      "invalid revisions and out-of-domain or empty intervals never construct coverage");
  const auto ordinal =
      SourceCoverage::FromSources("source-a", 100, std::array<uint32_t, 4>{1, 2, 7, 90});
  CHECK(ordinal && ordinal->Ranges().size() == 3 &&
            ordinal->Ranges()[0] == (Range{.First = 1, .Count = 2}),
        "native source ordinals canonicalize without a cell mask");
  CHECK(!SourceCoverage::FromSources("source-a", 10, std::array<uint32_t, 2>{1, 1}) &&
            !SourceCoverage::FromSources("source-a", 10, std::array<uint32_t, 2>{2, 1}),
        "duplicate and unordered ordinal lists are invalid");
  const uint64_t most = std::numeric_limits<uint64_t>::max();
  const auto edge =
      SourceCoverage::FromRanges("edge", most, std::array{Range{.First = most - 2, .Count = 2}});
  CHECK(edge && SourceCoverage::Partition(*edge, std::array{&*edge}) &&
            !SourceCoverage::FromRanges(
                "edge", most, std::array{Range{.First = most - 1, .Count = 2}}),
        "maximum domains validate without endpoint overflow");
  const auto empty = SourceCoverage::FromRanges("empty", 0, {});
  CHECK(empty && SourceCoverage::Partition(*empty, {}),
        "an explicitly empty domain needs no rendering products");
  return Report();
}
