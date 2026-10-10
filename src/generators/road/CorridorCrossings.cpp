#include "Corridors.h"
#include "Digest.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators {
namespace {

constexpr double kCrossCellM = 32.0;
constexpr double kCrossingToleranceM = 0.02;
constexpr uint64_t kRoadLevelNodeTag = 1ull << 63u;

uint64_t CrossingCell(EastNorth at) {
  const auto east = static_cast<int64_t>(std::floor(at.EastM / kCrossCellM));
  const auto north = static_cast<int64_t>(std::floor(at.NorthM / kCrossCellM));
  return (static_cast<uint64_t>(east + 0x20000000LL) << 32U) |
         static_cast<uint64_t>(north + 0x20000000LL);
}

}

uint64_t Corridors::RoadPositionKey(LongitudeLatitude at) {
  constexpr double kStepsPerDegree = 10'000'000;
  const auto north = std::llround(at.LatitudeDeg * kStepsPerDegree) + 1'000'000'000LL;
  const auto east = std::llround(at.LongitudeDeg * kStepsPerDegree) + 2'000'000'000LL;
  return (static_cast<uint64_t>(north) << 32u) | static_cast<uint64_t>(east);
}

uint64_t Corridors::RoadNodeAt(const ::outshine::Generators::Osm::StreetField::Way &lane,
                               LongitudeLatitude at) {
  const uint64_t key = RoadPositionKey(at);
  if (lane.Layer == 0 && !lane.Bridge) { return key; }
  uint64_t digest = kDigestBasis;
  for (unsigned shift = 0; shift < 64; shift += kByteBits) {
    digest = DigestFolded(digest, static_cast<uint8_t>(key >> shift));
  }
  for (unsigned shift = 0; shift < 32; shift += kByteBits) {
    digest = DigestFolded(digest, static_cast<uint8_t>(static_cast<uint32_t>(lane.Layer) >> shift));
  }
  return DigestFolded(digest, lane.Bridge ? 1u : 0u) | kRoadLevelNodeTag;
}

void Corridors::FileCrossing(const Path::Network::Crossing &one,
                             const Paving &on,
                             const Path::Network &network,
                             Paved &into) {
  const size_t first = network.TagOf(one.OverWay);
  const size_t second = network.TagOf(one.UnderWay);
  const auto &a = on.Ways.Ways()[first];
  const auto &b = on.Ways.Ways()[second];
  const LongitudeLatitude at{.LongitudeDeg = one.LongitudeDeg, .LatitudeDeg = one.LatitudeDeg};
  const auto crossedAt = GroundUnder(on, at);
  if (!crossedAt) { return; }
  const auto crossing = static_cast<uint32_t>(into.Crossings.size());
  into.Crossings.push_back({.EastM = crossedAt->EastM,
                            .NorthM = crossedAt->NorthM,
                            .GradeM = crossedAt->GradeM,
                            .Nodes = {PlannedNodeAt(on, a, at), PlannedNodeAt(on, b, at)},
                            .Lanes = {first, second}});
  for (int stepE = -1; stepE <= 1; ++stepE) {
    for (int stepN = -1; stepN <= 1; ++stepN) {
      const uint64_t cell = CrossingCell({.EastM = crossedAt->EastM + stepE * kCrossCellM,
                                          .NorthM = crossedAt->NorthM + stepN * kCrossCellM});
      into.AtCrossing[cell].push_back(crossing);
    }
  }
}

void Corridors::BindCrossingStations(size_t laneAt, Paved &into) {
  for (auto &station : into.Along) {
    if (station.Node != 0) { continue; }
    const auto cell =
        into.AtCrossing.find(CrossingCell({.EastM = station.EastM, .NorthM = station.NorthM}));
    if (cell == into.AtCrossing.end()) { continue; }
    for (const uint32_t crossing : cell->second) {
      const Crossing &met = into.Crossings[crossing];
      if (met.Lanes[0] != laneAt && met.Lanes[1] != laneAt) { continue; }
      if (std::hypot(station.EastM - met.EastM, station.NorthM - met.NorthM) >
          kCrossingToleranceM) {
        continue;
      }
      station.EastM = met.EastM;
      station.NorthM = met.NorthM;
      station.GradeM = met.GradeM;
      station.Node = met.Nodes[met.Lanes[0] == laneAt ? 0 : 1];
      break;
    }
  }
}

std::optional<RoadStation> Corridors::CrossingStation(size_t laneAt,
                                                      const Crossing &met,
                                                      std::span<const RoadStation, 2> span) {
  const auto &begin = span.front();
  const auto &end = span.back();
  const double runE = end.EastM - begin.EastM;
  const double runN = end.NorthM - begin.NorthM;
  const double lengthSquared = runE * runE + runN * runN;
  if (!(lengthSquared > 0.0)) { return std::nullopt; }
  if (met.Lanes[0] != laneAt && met.Lanes[1] != laneAt) { return std::nullopt; }
  const double along =
      ((met.EastM - begin.EastM) * runE + (met.NorthM - begin.NorthM) * runN) / lengthSquared;
  if (along <= 0.0 || along >= 1.0) { return std::nullopt; }
  const double offE = met.EastM - std::lerp(begin.EastM, end.EastM, along);
  const double offN = met.NorthM - std::lerp(begin.NorthM, end.NorthM, along);
  if (std::hypot(offE, offN) > kCrossingToleranceM) { return std::nullopt; }
  if (std::min(along, 1.0 - along) * std::sqrt(lengthSquared) <= kCrossingToleranceM) {
    return std::nullopt;
  }
  return RoadStation{.EastM = met.EastM,
                     .NorthM = met.NorthM,
                     .GradeM = met.GradeM,
                     .Node = met.Nodes[met.Lanes[0] == laneAt ? 0 : 1]};
}

void Corridors::SplitAtCrossings(size_t laneAt, Paved &into) {
  if (into.AtCrossing.empty()) { return; }
  BindCrossingStations(laneAt, into);
  into.Finer.clear();
  std::vector<RoadStation> cuts;
  for (size_t station = 1; station < into.Along.size(); ++station) {
    const auto &begin = into.Along[station - 1];
    const auto &end = into.Along[station];
    into.Finer.push_back(begin);
    const auto cell = into.AtCrossing.find(CrossingCell(
        {.EastM = 0.5 * (begin.EastM + end.EastM), .NorthM = 0.5 * (begin.NorthM + end.NorthM)}));
    if (cell == into.AtCrossing.end()) { continue; }
    const double runE = end.EastM - begin.EastM;
    const double runN = end.NorthM - begin.NorthM;
    cuts.clear();
    for (const uint32_t crossing : cell->second) {
      const Crossing &met = into.Crossings[crossing];
      const auto cut = CrossingStation(
          laneAt, met, std::span<const RoadStation, 2>{into.Along.data() + station - 1, 2});
      if (cut) { cuts.push_back(*cut); }
    }
    std::ranges::sort(cuts, [&](const auto &a, const auto &b) {
      const double forward = (a.EastM - b.EastM) * runE + (a.NorthM - b.NorthM) * runN;
      return forward != 0.0 ? forward < 0.0 : a.Node < b.Node;
    });
    for (const auto &cut : cuts) {
      if (into.Finer.back().Node != cut.Node) { into.Finer.push_back(cut); }
    }
  }
  into.Finer.push_back(into.Along.back());
  into.Along.swap(into.Finer);
}

}
