#include "Wayfinding.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr size_t kWays = 1025;
constexpr size_t kTotalWays = kWays + 2u;
constexpr double kStepDeg = 0.0001;
constexpr outshine::LongitudeLatitude kStart{.LongitudeDeg = 0, .LatitudeDeg = 0};
constexpr outshine::LongitudeLatitude kFinish{.LongitudeDeg = kWays * kStepDeg, .LatitudeDeg = 0};

std::expected<outshine::Path::Network, std::string_view> Fixture(bool reversed) {
  using namespace outshine;
  auto made = Path::Network::Create({.CellM = 10}, {});
  if (!made) { return made; }
  for (size_t at = 0; at < kWays; ++at) {
    const size_t which = reversed ? kWays - 1u - at : at;
    const double west = static_cast<double>(which) * kStepDeg;
    const std::array<double, 4> points{0, west, 0, west + kStepDeg};
    if (auto laid = made->Lay(points, {.HalfWidthM = 1, .Tag = which + 1u}); !laid) {
      return std::unexpected(laid.error());
    }
  }
  const std::array<double, 4> duplicate{0, 511.0 * kStepDeg, 0, 512.0 * kStepDeg};
  for (const size_t tag : {kWays + 1u, kWays + 2u}) {
    if (auto laid = made->Lay(duplicate, {.HalfWidthM = 1, .Tag = tag}); !laid) {
      return std::unexpected(laid.error());
    }
  }
  return made;
}

void CheckSlice(const outshine::Path::Network &oracle,
                const outshine::Path::Route &route,
                bool reversed,
                size_t budget) {
  using namespace outshine;
  using namespace outshine::Test;
  auto input = Fixture(reversed);
  CHECK(input.has_value(), "large sliced input is accepted");
  if (!input) { return; }
  auto job = Path::NetworkWeaveJob::Begin(std::move(*input));
  CHECK(job.has_value(), "large sliced weave starts");
  if (!job) { return; }
  bool done = false;
  for (size_t step = 0; step < 100000 && !done; ++step) {
    auto advanced = job->Advance(budget);
    CHECK(advanced.has_value(), "large sliced weave advances");
    if (!advanced) { break; }
    done = *advanced;
  }
  CHECK(done, "all bounded ordering, merge and copy phases finish");
  if (!done) { return; }
  auto woven = std::move(*job).Take();
  CHECK(woven.has_value(), "complete large graph transfers");
  if (!woven) { return; }
  bool sameTags = true;
  for (size_t at = 0; at < kTotalWays; ++at) {
    sameTags = sameTags && woven->TagOf(at) == oracle.TagOf(at);
  }
  const Path::Route actual = woven->Plan(kStart, kFinish, 0);
  CHECK(sameTags && woven->NodeCount() == oracle.NodeCount() &&
            woven->EdgeCount() == oracle.EdgeCount() && actual.Found == route.Found &&
            actual.LengthM == route.LengthM && actual.Legs.size() == route.Legs.size(),
        "sliced graph IDs, topology and route match one-shot across three sort runs");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const bool reversed : {false, true}) {
    auto oracle = Fixture(reversed);
    CHECK(oracle.has_value(), "large line fixture is accepted");
    if (!oracle) { continue; }
    std::string error;
    CHECK(oracle->Weave(error), "large one-shot graph builds");
    CHECK(oracle->TagOf(511) == 512 && oracle->TagOf(512) == kWays + 1u &&
              oracle->TagOf(513) == kWays + 2u,
          "duplicate geometry at a sort-run boundary has deterministic tag order");
    const Path::Route route = oracle->Plan(kStart, kFinish, 0);
    CHECK(route.Found, "one-shot graph follows the full line");
    for (const size_t budget : {size_t{1}, size_t{31}, size_t{4096}}) {
      CheckSlice(*oracle, route, reversed, budget);
    }
  }
  return Report();
}
