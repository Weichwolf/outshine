#include "Wayfinding.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::expected<outshine::Path::Network, std::string_view> Fixture() {
  using namespace outshine;
  auto created = Path::Network::Create({.CellM = 1}, {});
  if (!created) { return created; }
  const std::array<double, 4> road{0, 0, 0, 0.02};
  const std::array<double, 4> spur{0, 0.01, -0.01, 0.01};
  const std::array<double, 4> anchor{-0.01, 0.02, 0, 0.02};
  for (const auto &points : {road, spur, anchor}) {
    if (auto laid = created->Lay(points, {.HalfWidthM = 1, .Oneway = true}); !laid) {
      return std::unexpected(laid.error());
    }
  }
  return created;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto oracle = Fixture();
  CHECK(oracle.has_value(), "directed crossing fixture accepted");
  if (!oracle) { return Report(); }
  std::string error;
  CHECK(oracle->Weave(error), "one-shot oracle completes the fixture");
  const LongitudeLatitude west{.LongitudeDeg = 0, .LatitudeDeg = 0};
  const LongitudeLatitude east{.LongitudeDeg = 0.02, .LatitudeDeg = 0};
  const LongitudeLatitude south{.LongitudeDeg = 0.01, .LatitudeDeg = -0.01};
  CHECK(oracle->TiedToEdges() == 1 && oracle->Plan(west, east, 0).Found &&
            !oracle->Plan(east, west, 0).Found && !oracle->Plan(south, east, 0).Found,
        "oracle includes a directed physical splice");

  for (const size_t budget : {1u, 2u, 8u}) {
    auto input = Fixture();
    CHECK(input.has_value(), "sliced fixture accepted");
    if (!input) { continue; }
    auto started = Path::NetworkWeaveJob::Begin(std::move(*input));
    CHECK(started.has_value(), "sliced weave starts");
    if (!started) { continue; }
    CHECK(!std::move(*started).Take().has_value(), "incomplete graph cannot publish");
    CHECK(!started->Advance(0).has_value(), "zero work budget is rejected");
    bool done = false;
    for (size_t step = 0; step < 10000 && !done; ++step) {
      const auto advanced = started->Advance(budget);
      CHECK(advanced.has_value(), "sliced weave advances without a construction error");
      if (!advanced) { break; }
      done = *advanced;
    }
    CHECK(done, "bounded slices finish the graph");
    if (!done) { continue; }
    auto woven = std::move(*started).Take();
    CHECK(woven.has_value(), "complete graph transfers its owner");
    if (!woven) { continue; }
    CHECK(woven->NodeCount() == oracle->NodeCount() && woven->EdgeCount() == oracle->EdgeCount() &&
              woven->TiedToEdges() == oracle->TiedToEdges(),
          "slicing preserves native topology counts");
    for (const auto [from, to] : {std::pair{west, east},
                                  std::pair{east, west},
                                  std::pair{south, east},
                                  std::pair{west, south}}) {
      const Path::Route expected = oracle->Plan(from, to, 0);
      const Path::Route actual = woven->Plan(from, to, 0);
      CHECK(actual.Found == expected.Found && actual.LengthM == expected.LengthM &&
                actual.Legs.size() == expected.Legs.size(),
            "directed routes and station lengths match the one-shot oracle");
    }
  }
  auto empty = Path::Network::Create({.CellM = 1}, {});
  CHECK(empty.has_value(), "empty graph can be configured");
  if (empty) {
    CHECK(!Path::NetworkWeaveJob::Begin(std::move(*empty)).has_value(),
          "empty graph cannot start a weave job");
  }
  return Report();
}
