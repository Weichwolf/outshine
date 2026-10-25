#include "Wayfinding.h"
#include "Earth.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <expected>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

std::expected<Path::Network, std::string_view> Fixture(bool reversed) {
  auto made = Path::Network::Create({.CellM = 1}, {});
  if (!made) { return made; }
  constexpr std::array<double, 4> road{0, 0, 0, 0.02};
  constexpr std::array<double, 4> spur{0, 0.01, -0.01, 0.01};
  const std::array points{road, spur, road};
  const std::array<size_t, 3> order =
      reversed ? std::array<size_t, 3>{2, 1, 0} : std::array<size_t, 3>{0, 1, 2};
  for (const size_t at : order) {
    const auto laid = made->Lay(points[at], {.HalfWidthM = 1, .Oneway = at == 0, .Tag = at + 11});
    if (!laid) { return std::unexpected(laid.error()); }
  }
  return made;
}

void CheckSources(const Path::Network &network) {
  std::array<size_t, 3> forward{};
  std::array<size_t, 3> reverse{};
  size_t counted = 0;
  for (size_t node = 0; node < network.NodeCount(); ++node) {
    CHECK(network.PositionOfNode(node).has_value(), "published nodes retain their native location");
    for (const auto &edge : network.ConnectionsFrom(node)) {
      const size_t tag = network.TagOf(edge.Source.Way);
      CHECK(tag >= 11 && tag <= 13 && edge.Source.Step() == 1 &&
                network.PositionOfNode(edge.To).has_value(),
            "each actual connection names its original way and directed source segment");
      if (tag < 11 || tag > 13) { continue; }
      ++(edge.Source.Reverse() ? reverse[tag - 11] : forward[tag - 11]);
      ++counted;
    }
  }
  constexpr std::array<size_t, 3> expectedForward{2, 1, 2};
  constexpr std::array<size_t, 3> expectedReverse{0, 1, 2};
  CHECK(counted == network.EdgeCount() && network.TiedToEdges() == 1 &&
            forward == expectedForward && reverse == expectedReverse,
        "physical splice preserves both duplicate owners and every original travel direction");
  CHECK(!network.PositionOfNode(network.NodeCount()) &&
            network.ConnectionsFrom(network.NodeCount()).empty(),
        "an invalid node index cannot expose unrelated native storage");
}

void CheckRoundTrip(const Path::Network &network) {
  constexpr size_t bytesMost = size_t{1024} * 1024;
  const auto encoded = network.EncodeAsset(bytesMost);
  CHECK(encoded.has_value(), "source-bearing native connections fit the asset allowance");
  if (!encoded) { return; }
  const auto decoded = Path::Network::DecodeAsset(*encoded, bytesMost);
  CHECK(decoded.has_value(), "source-bearing connections load without another topology build");
  if (!decoded) { return; }
  CheckSources(*decoded);
  for (size_t node = 0; node < network.NodeCount(); ++node) {
    const auto expected = network.ConnectionsFrom(node);
    const auto actual = decoded->ConnectionsFrom(node);
    CHECK(actual.size() == expected.size(), "loaded connection ranges keep their native counts");
    if (actual.size() != expected.size()) { continue; }
    for (size_t at = 0; at < actual.size(); ++at) {
      CHECK(actual[at].To == expected[at].To && actual[at].LengthM == expected[at].LengthM &&
                actual[at].Source.Way == expected[at].Source.Way &&
                actual[at].Source.SignedStep == expected[at].Source.SignedStep,
            "actual edge endpoints, length and precise source reference survive serialization");
    }
  }
  auto oldFormat = *encoded;
  oldFormat[4] = 1;
  CHECK(!Path::Network::DecodeAsset(oldFormat, bytesMost),
        "an older source-free private network format cannot qualify the new publication");
  std::println(
      "NETWORK_CONNECTION_SOURCES edges={} connection_bytes={} held_bytes={} asset_bytes={}",
      network.EdgeCount(),
      sizeof(Path::Network::Connection),
      network.ConnectionCapacityBytes(),
      encoded->size());
}
}

int main() {
  for (const bool reversed : {false, true}) {
    auto network = Fixture(reversed);
    CHECK(network.has_value(), "source-bearing physical splice fixture is accepted");
    if (!network) { continue; }
    CHECK(network->ConnectionsFrom(0).empty() && !network->PositionOfNode(0),
          "an unpublished graph cannot leak an unfinished topology view");
    std::string error;
    CHECK(network->Weave(error), "the existing topology builder completes");
    CheckSources(*network);
    const auto elevated = network->Elevate([](LongitudeLatitude) { return 0.0; });
    CHECK(elevated.Refused == 0, "existing height preparation completes without another graph");
    CheckRoundTrip(*network);
  }
  return Report();
}
