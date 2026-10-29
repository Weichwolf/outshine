#include "MvtTransportSnapshotInternal.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;

void CheckIndependentWire() {
  World::TransportTopology topology(
      {.DatasetId = "wire-vector", .Revision = std::string(64, 'a')}, {}, {});
  MvtTransportSourceFacts facts;
  facts.Network = std::make_shared<World::TransportNetworkSnapshot>(
      World::ResolvedTransport{.Graph = std::move(topology), .Routes = {}},
      std::vector<GeographicBounds>{},
      World::TransportLoadMetrics{});
  auto groups = std::make_shared<MvtTransportNodeGroups>();
  for (size_t at = 0; at < groups->Digest.size(); ++at) {
    groups->Digest[at] = static_cast<uint8_t>(at);
  }
  facts.NodeGroups = std::move(groups);
  facts.SourceGeneration = 7;
  facts.SourceWayCount = 11;
  facts.Groups = {{.FirstMember = 0,
                   .MemberCount = 1,
                   .OriginalEndpointCount = 2,
                   .FilteredEndpointCount = 1,
                   .Complete = false,
                   .Frontier = true}};
  facts.Members = {{.Edge = {.PathId = 0x0102030405060708,
                             .SegmentOrdinal = 3,
                             .Direction = World::EdgeDirection::Reverse},
                    .DomainNode = 0x1112131415161718,
                    .NodeGroup = 0,
                    .StreetWay = 7,
                    .SourceStep = 4,
                    .End = 1,
                    .SourceTerminal = true}};
  MvtTransportWayFacts way{.StreetWay = 7,
                           .Feature = 8,
                           .Ring = 9,
                           .PointCount = 6,
                           .HalfWidthWord = 0x3fe00000,
                           .MaximumGradientWord = 0x3df5c28f,
                           .OwnsEnds = {true, false}};
  for (size_t at = 0; at < 4; ++at) {
    way.EndStencil[at] = {.LongitudeDeg = 16.0 + static_cast<double>(at) * 0.001,
                          .LatitudeDeg = 48.0};
  }
  facts.Ways.push_back(way);
  facts.OmissionsDigest = {132, 64,  123, 78,  78,  18,  190, 124, 189, 47,  162,
                           16,  3,   78,  202, 184, 237, 83,  137, 200, 222, 131,
                           223, 147, 183, 100, 54,  81,  54,  20,  5,   165};
  MvtTransportSourceFacts::MissingWay missing{
      .Omission = 0, .Way = way, .Layer = -1, .Points = {}, .OriginalPoints = {}};
  missing.Way.StreetWay = 3;
  missing.Way.PointCount = 5;
  for (size_t at = 0; at < 5; ++at) {
    missing.Points.push_back({.LongitudeDeg = 16.0 + static_cast<double>(at) * 0.001,
                              .LatitudeDeg = 48.0 + (at == 2 ? 0.001 : 0.0)});
  }
  constexpr std::array<size_t, 4> stencil{0, 1, 3, 4};
  for (size_t at = 0; at < stencil.size(); ++at) {
    missing.Way.EndStencil[at] = missing.Points[stencil[at]];
  }
  missing.OriginalWayState = MvtTransportOriginalWayState::Exact;
  missing.OriginalPoints.resize(5);
  missing.OriginalPoints[1].State =
      MvtTransportOriginalPointState::ValidNativeNodeWithoutPublishedGroup;
  missing.OriginalPoints[2] = {.State = MvtTransportOriginalPointState::PublishedGroup, .Group = 0};
  missing.OriginalPoints[4].State =
      MvtTransportOriginalPointState::ValidNativeNodeWithoutPublishedGroup;
  facts.MissingWays.push_back(std::move(missing));
  constexpr std::array<uint8_t, 32> expected{145, 244, 213, 91,  92,  206, 241, 59,  189, 193, 118,
                                             113, 93,  209, 218, 42,  108, 206, 118, 221, 68,  105,
                                             47,  214, 188, 176, 140, 167, 120, 199, 119, 206};
  MvtTransport::BindSourceFacts(facts);
  CHECK(facts.Digest == expected,
        "native v4 digest matches Research's independent 576-byte LE vector and OpenSSL pin");
  for (const int change : {0, 1, 2, 3}) {
    auto altered = facts;
    auto &row = altered.MissingWays.front();
    if (change == 0) { row.OriginalWayState = MvtTransportOriginalWayState::AmbiguousExact; }
    if (change == 1) {
      row.OriginalPoints[0].State =
          MvtTransportOriginalPointState::ValidNativeNodeWithoutPublishedGroup;
    }
    if (change == 2) { row.OriginalPoints[2].Group = 1; }
    if (change == 3) { row.OriginalPoints.pop_back(); }
    MvtTransport::BindSourceFacts(altered);
    CHECK(altered.Digest != expected,
          "way state, ordered point state, group and record count all affect the native digest");
  }
}
}

int main() {
  CheckIndependentWire();
  return Report();
}
