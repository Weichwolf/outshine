#include "Wayfinding.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto fixture = [] {
    auto made = Path::Network::Create({.CellM = 1}, {});
    if (!made) { return made; }
    constexpr std::array<double, 4> across{0, -0.02, 0, 0.02};
    constexpr std::array<double, 4> centre{-0.02, 0, 0.02, 0};
    constexpr std::array<double, 4> offset{-0.02, 0.01, 0.02, 0.01};
    if (!made->Lay(across, {}) || !made->Lay(centre, {}) || !made->Lay(offset, {})) {
      return Path::Network::Create({.CellM = 0}, {});
    }
    return made;
  };

  auto oracle = fixture();
  CHECK(oracle.has_value(), "crossing fixture accepted");
  if (!oracle) { return Report(); }
  std::vector<Path::Network::Crossing> expected;
  const auto swept = oracle->Crossings(expected);
  CHECK(swept && swept->Found == 2, "one-shot oracle finds both crossings");

  for (const size_t budget : {size_t{1}, size_t{2}, size_t{8}}) {
    auto input = fixture();
    CHECK(input.has_value(), "sliced crossing fixture accepted");
    if (!input) { continue; }
    auto job = Path::NetworkCrossingJob::Begin(std::move(*input));
    CHECK(job.has_value(), "crossing job starts");
    if (!job) { continue; }
    CHECK(!job->Advance(0).has_value(), "zero pair budget is rejected");
    bool done = false;
    for (size_t step = 0; step < 10000 && !done; ++step) {
      const auto advanced = job->Advance(budget);
      CHECK(advanced.has_value(), "crossing pair slice advances");
      if (!advanced) { break; }
      done = *advanced;
    }
    CHECK(done, "crossing pair slices complete");
    auto result = std::move(*job).Take();
    CHECK(result.has_value(), "completed crossing graph transfers its owner");
    if (!result) { continue; }
    std::vector<Path::Network::Crossing> actual;
    const auto measured = result->Graph.Crossings(actual);
    CHECK(measured && measured->Found == swept->Found &&
              measured->PairsTested == swept->PairsTested &&
              measured->PairsPruned == swept->PairsPruned &&
              measured->FullestCell == swept->FullestCell &&
              measured->CandidatePairs == swept->CandidatePairs,
          "sliced crossing statistics match one-shot classification");
    CHECK(actual.size() == expected.size(), "sliced crossing count matches one-shot result");
    for (size_t at = 0; at < actual.size() && at < expected.size(); ++at) {
      CHECK(actual[at].OverWay == expected[at].OverWay &&
                actual[at].UnderWay == expected[at].UnderWay &&
                actual[at].LatitudeDeg == expected[at].LatitudeDeg &&
                actual[at].LongitudeDeg == expected[at].LongitudeDeg &&
                actual[at].OverAt == expected[at].OverAt &&
                actual[at].UnderAt == expected[at].UnderAt,
            "sliced crossing identity matches one-shot result");
    }
  }
  return Report();
}
