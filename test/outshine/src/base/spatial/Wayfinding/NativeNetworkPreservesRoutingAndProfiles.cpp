#include "Wayfinding.h"
#include "Check.h"
#include "Geodesy.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t bytesMost = 1024 * 1024;
  auto created = Path::Network::Create({.CellM = 1}, {});
  CHECK(created.has_value(), "network grid is valid");
  if (!created) { return Report(); }
  auto &network = *created;
  auto empty = network.EncodeAsset(bytesMost);
  CHECK(empty && Path::Network::DecodeAsset(*empty, bytesMost),
        "empty regions remain native products");
  constexpr std::array<double, 6> along{0, 0, 0.01, 0, 0.03, 0};
  constexpr std::array<double, 4> crossing{0.005, -0.01, 0.005, 0.01};
  CHECK(network.Lay(along,
                    {.HalfWidthM = 2,
                     .MaxGradient = 0.1,
                     .MinRadiusM = 5,
                     .Friction = 0.8,
                     .SpeedMps = 12,
                     .Lanes = 2,
                     .Oneway = true,
                     .Sealed = true,
                     .Tag = 17})
                .has_value() &&
            network.Lay(crossing, {.HalfWidthM = 3, .Spans = true, .Tag = 29}).has_value(),
        "directed road and overpass are accepted");
  std::string error;
  CHECK(network.Weave(error), "native topology is woven");
  CHECK(!network.EncodeAsset(bytesMost), "unprepared height profiles cannot be published");
  std::vector<Path::Network::Crossing> crossings;
  const auto sweep = network.Crossings(crossings);
  CHECK(sweep && sweep->Found == 1, "overpass crossing is classified");
  const auto elevated = network.Elevate(
      [](LongitudeLatitude at) { return std::optional<double>(100 + at.LatitudeDeg * 1000); });
  CHECK(elevated.Points == network.PointCount() && elevated.Refused == 0,
        "all heights are prepared");
  auto encoded = network.EncodeAsset(bytesMost);
  CHECK(encoded.has_value(), "complete road product encodes");
  if (!encoded) { return Report(); }
  auto decoded = Path::Network::DecodeAsset(*encoded, encoded->size());
  CHECK(decoded.has_value(), "native product decodes within its exact payload allowance");
  if (!decoded) { return Report(); }
  CHECK(decoded->BoundsEcef() == network.BoundsEcef(), "native spatial bounds survive loading");
  for (const double latitude : {0.0, 0.005, 0.01, 0.02, 0.03}) {
    Vec3 ecef;
    GeoToEcef({.LongitudeDeg = 0, .LatitudeDeg = latitude, .HeightM = 100 + latitude * 1000}, ecef);
    CHECK(decoded->BoundsEcef().Holds(ecef),
          "spatial bounds contain both native points and intervening terrain profiles");
  }
  CHECK(decoded->WayCount() == network.WayCount() && decoded->NodeCount() == network.NodeCount() &&
            decoded->EdgeCount() == network.EdgeCount() &&
            decoded->JunctionCount() == network.JunctionCount() &&
            decoded->CrossingsJoined() == network.CrossingsJoined() &&
            decoded->CrossingsLeftAlone() == network.CrossingsLeftAlone() &&
            decoded->TiedToEdges() == network.TiedToEdges(),
        "all topology counts survive");
  for (size_t way = 0; way < network.WayCount(); ++way) {
    CHECK(decoded->TagOf(way) == network.TagOf(way) &&
              decoded->LengthM(way) == network.LengthM(way),
          "way identities and lengths survive");
    const auto expected = network.Profile({.Way = way, .StationM = network.LengthM(way) / 2});
    const auto actual = decoded->Profile({.Way = way, .StationM = decoded->LengthM(way) / 2});
    CHECK(expected && actual && expected->HeightM == actual->HeightM &&
              expected->Grade == actual->Grade,
          "loaded profiles need no terrain provider");
  }
  const LongitudeLatitude goal{.LatitudeDeg = 0.03};
  const auto route = decoded->Plan({}, goal, 0);
  CHECK(route.Found && route.Legs.size() == 3 && route.Legs[0].HalfWidthM == 2 &&
            route.Legs[0].Friction == 0.8 && route.Legs[0].Lanes == 2,
        "loaded routing preserves connectivity and physical attributes");
  CHECK(!decoded->Plan(goal, {}, 0).Found, "loaded directed road rejects reverse travel");
  const auto nearest = decoded->Nearest(goal);
  const auto originalNearest = network.Nearest(goal);
  CHECK(nearest && originalNearest && *nearest && *originalNearest &&
            (**nearest).Node == (**originalNearest).Node &&
            (**nearest).AwayM == (**originalNearest).AwayM,
        "spatial search returns the original node");
  std::vector<size_t> nearby, expectedNearby;
  CHECK(decoded->Within(goal, 100, nearby).has_value() &&
            network.Within(goal, 100, expectedNearby).has_value() && nearby == expectedNearby,
        "loaded spatial index answers radius requests");
  std::vector<Path::Network::Crossing> actualCrossings;
  const auto actualSweep = decoded->Crossings(actualCrossings);
  CHECK(actualSweep && actualCrossings.size() == crossings.size(),
        "crossings load without reclassification");
  if (!actualCrossings.empty() && !crossings.empty()) {
    CHECK(actualCrossings[0].OverAt == crossings[0].OverAt &&
              actualCrossings[0].UnderAt == crossings[0].UnderAt &&
              actualCrossings[0].LatitudeDeg == crossings[0].LatitudeDeg &&
              actualCrossings[0].LongitudeDeg == crossings[0].LongitudeDeg,
          "crossing segments and coordinates survive");
  }
  CHECK(!network.EncodeAsset(encoded->size() - 1) &&
            !Path::Network::DecodeAsset(*encoded, encoded->size() - 1),
        "payload budget is enforced in both directions");
  for (size_t length : {size_t{0}, size_t{8}, encoded->size() / 2, encoded->size() - 1}) {
    CHECK(!Path::Network::DecodeAsset(std::span(*encoded).first(length), bytesMost),
          "truncated products are refused");
  }
  auto corrupt = *encoded;
  corrupt[0] ^= 1;
  CHECK(!Path::Network::DecodeAsset(corrupt, bytesMost), "unknown versions are refused");
  corrupt = *encoded;
  corrupt.push_back(0);
  CHECK(!Path::Network::DecodeAsset(corrupt, bytesMost), "trailing bytes are refused");
  corrupt = *encoded;
  std::fill_n(corrupt.begin() + 57, 4, uint8_t{255});
  CHECK(!Path::Network::DecodeAsset(corrupt, bytesMost),
        "forged point counts are refused before allocation");
  return Report();
}
