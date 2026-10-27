#include "MvtTransportSourceFactsInternal.h"
#include "OsmField.h"
#include "StreetField.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
std::expected<MvtTransportWayFacts, std::string> WayFacts(const SourceFactInput &input,
                                                          uint32_t at) {
  const auto &way = input.Streets.Ways()[at];
  const auto source = input.Streets.SourceOf(input.Field, at);
  const auto points = input.Field.Points();
  const size_t first = way.FirstPoint;
  if (!source || way.PointCount < 2 || first > points.size() / 2 ||
      way.PointCount > points.size() / 2 - first) {
    return std::unexpected("MVT source facts lost their actual source way binding");
  }
  MvtTransportWayFacts facts{.StreetWay = at,
                             .Feature = source->Feature,
                             .Ring = source->Ring,
                             .PointCount = way.PointCount,
                             .HalfWidthWord = std::bit_cast<uint32_t>(way.HalfWidthM),
                             .MaximumGradientWord = std::bit_cast<uint32_t>(way.MaxGradient),
                             .OwnsEnds = way.OwnsEnds};
  const size_t last = first + way.PointCount - 1;
  const std::array<size_t, 4> ends{first, first + 1, last - 1, last};
  for (size_t end = 0; end < ends.size(); ++end) {
    facts.EndStencil[end] = {.LongitudeDeg = points[2 * ends[end] + 1],
                             .LatitudeDeg = points[2 * ends[end]]};
  }
  return facts;
}
}

std::expected<void, std::string> FillSourceWayFacts(MvtTransportSourceFacts &facts,
                                                    const SourceFactInput &input,
                                                    const std::stop_token &stop) {
  auto ways = input.SourceRows.StreetWays;
  std::ranges::sort(ways);
  const auto duplicate = std::ranges::unique(ways);
  ways.erase(duplicate.begin(), duplicate.end());
  facts.Ways.reserve(input.SourceRows.StreetWays.size());
  for (const uint32_t way : ways) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    auto captured = WayFacts(input, way);
    if (!captured) { return std::unexpected(captured.error()); }
    facts.Ways.push_back(*captured);
  }
  for (const auto &member : facts.Members) {
    const auto way = std::ranges::lower_bound(
        facts.Ways, member.StreetWay, {}, &MvtTransportWayFacts::StreetWay);
    const size_t physical = member.End ^ static_cast<uint8_t>(member.Edge.Direction);
    if (member.SourceTerminal && !way->OwnsEnds[physical]) {
      facts.Groups[member.NodeGroup].Frontier = true;
    }
  }
  return {};
}
}
