#include "Wayfinding.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::expected<outshine::Path::Network, std::string_view> Fixture() {
  using namespace outshine;
  auto created = Path::Network::Create({.CellM = 1}, {});
  if (!created) { return created; }
  const std::array<double, 6> north{0, 0, 0.01, 0, 0.02, 0};
  const std::array<double, 4> east{0, 0, 0, 0.01};
  if (auto laid = created->Lay(north, {.Sealed = true}); !laid) {
    return std::unexpected(laid.error());
  }
  if (auto laid = created->Lay(east, {}); !laid) { return std::unexpected(laid.error()); }
  std::string error;
  if (!created->Weave(error)) { return std::unexpected("fixture weave failed"); }
  return created;
}

std::optional<double> HeightAt(outshine::LongitudeLatitude at) {
  if (at.LatitudeDeg == 0 && at.LongitudeDeg == 0) { return std::nullopt; }
  return 100.0 + 1000.0 * at.LatitudeDeg + 2000.0 * at.LongitudeDeg;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto oracle = Fixture();
  CHECK(oracle.has_value(), "shared-node fixture built");
  if (!oracle) { return Report(); }
  const auto expected = oracle->Elevate(HeightAt);
  for (const size_t budget : {1u, 2u, 8u}) {
    auto input = Fixture();
    CHECK(input.has_value(), "sliced fixture built");
    if (!input) { continue; }
    size_t calls = 0;
    auto job = Path::NetworkElevationJob::Begin(std::move(*input), [&calls](LongitudeLatitude at) {
      ++calls;
      return HeightAt(at);
    });
    CHECK(!std::move(job).Take().has_value(), "incomplete elevation cannot publish");
    CHECK(!job.Advance(0).has_value(), "zero work budget is rejected");
    bool done = false;
    for (size_t step = 0; step < 100 && !done; ++step) {
      const auto advanced = job.Advance(budget);
      CHECK(advanced.has_value(), "sliced elevation advances");
      if (!advanced) { break; }
      done = *advanced;
    }
    CHECK(done, "bounded slices finish elevation");
    if (!done) { continue; }
    auto made = std::move(job).Take();
    CHECK(made.has_value(), "complete elevation transfers its graph");
    if (!made) { continue; }
    CHECK(calls == made->Graph.NodeCount(), "each shared node is sampled once");
    CHECK(made->Statistics.Points == expected.Points &&
              made->Statistics.Refused == expected.Refused &&
              made->Statistics.SteepestGrade == expected.SteepestGrade &&
              made->Statistics.SealedOverTenPercent == expected.SealedOverTenPercent,
          "slicing preserves elevation diagnostics");
    for (size_t way = 0; way < 2; ++way) {
      CHECK(made->Graph.LengthM(way) == oracle->LengthM(way), "way station length is stable");
      for (const double along : {0.0, oracle->LengthM(way) * 0.5, oracle->LengthM(way)}) {
        const auto actual = made->Graph.Profile({.Way = way, .StationM = along});
        const auto wanted = oracle->Profile({.Way = way, .StationM = along});
        CHECK(actual && wanted && actual->HeightM == wanted->HeightM &&
                  actual->Grade == wanted->Grade,
              "height and grade match the one-shot oracle");
      }
    }
  }
  return Report();
}
