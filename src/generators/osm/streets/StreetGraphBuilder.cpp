#include "StreetGraphBuilder.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ratio>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "Earth.h"
#include "OsmField.h"
#include "StreetField.h"

namespace outshine::Generators::Osm {
namespace Says {
constexpr auto kInvalidStreetPointRange =
    "vector street point range exceeds the supplied coordinate stream";
}

namespace {
bool MakesWay(const StreetField::Way &lane) {
  return lane.Form == StreetField::Shape::Ribbon && lane.PointCount >= 2;
}

std::optional<std::span<const double>> WayPoints(const StreetField::Way &lane,
                                                 std::span<const double> points) {
  const size_t first = static_cast<size_t>(lane.FirstPoint) * 2;
  if (first > points.size() || lane.PointCount > (points.size() - first) / 2) {
    return std::nullopt;
  }
  return points.subspan(first, static_cast<size_t>(lane.PointCount) * 2);
}

Path::WayClass WayProperties(const StreetField::Way &lane, size_t tag) {
  return {.HalfWidthM = static_cast<double>(lane.HalfWidthM),
          .SpeedMps = static_cast<double>(lane.SpeedMps),
          .Lanes = lane.Lanes,
          .Priority = lane.Priority,
          .Oneway = lane.Oneway,
          .Sealed = lane.Sealed,
          .Spans = lane.Bridge,
          .Tag = tag};
}
}

StreetGraphBuilder::Built
StreetGraphBuilder::BuildOneShot(const ::outshine::Generators::Osm::StreetField &ways,
                                 std::span<const double> points,
                                 const Path::Network::HeightSource &heightOf) {
  Built made;
  auto created =
      Path::Network::Create(Path::Snap{.CellM = kNodeSnapM}, Path::Sphere{.RadiusM = kWgs84A});
  if (!created) {
    made.Refusal = created.error();
    return made;
  }
  auto graph = std::make_shared<Path::Network>(std::move(*created));
  auto phaseBegan = std::chrono::steady_clock::now();
  const auto phaseMs = [&phaseBegan] {
    const auto now = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(now - phaseBegan).count();
    phaseBegan = now;
    return ms;
  };
  if (const auto laid = LayWays(ways, points, *graph); !laid) {
    made.Refusal = laid.error();
    return made;
  }
  made.LayMs = phaseMs();
  made.Ways = graph->WayCount();
  if (made.Ways > 0 && !graph->Weave(made.Refusal, &made.WeavePhases)) { return made; }
  made.WeaveMs = phaseMs();
  std::vector<Path::Network::Crossing> crossings;
  if (const auto swept = graph->Crossings(crossings); !swept) {
    made.Refusal = swept.error();
    return made;
  }
  made.CrossingsMs = phaseMs();
  made.Nodes = graph->NodeCount();
  made.Edges = graph->EdgeCount();
  made.Junctions = graph->JunctionCount();
  made.Elevated = graph->Elevate(heightOf);
  made.ElevateMs = phaseMs();
  made.Graph = std::move(graph);
  return made;
}

std::expected<void, std::string_view>
StreetGraphBuilder::LayWays(const ::outshine::Generators::Osm::StreetField &ways,
                            std::span<const double> points,
                            Path::Network &graph) {
  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const ::outshine::Generators::Osm::StreetField::Way &lane = ways.Ways()[at];
    if (!MakesWay(lane)) { continue; }
    const auto coordinates = WayPoints(lane, points);
    if (!coordinates) { return std::unexpected(Says::kInvalidStreetPointRange); }
    const auto laid = graph.Lay(*coordinates, WayProperties(lane, at));
    if (!laid) { return laid; }
  }
  return {};
}

bool StreetGraphBuilder::MatchesWays(const StreetField &ways,
                                     std::span<const double> points,
                                     const Path::Network &graph) {
  if (!graph.UsesTopologyGrid(Path::Snap{.CellM = kNodeSnapM}, Path::Sphere{.RadiusM = kWgs84A}) ||
      graph.WayCount() != static_cast<size_t>(std::ranges::count_if(ways.Ways(), MakesWay))) {
    return false;
  }
  std::vector<uint8_t> seen(ways.Ways().size());
  for (size_t way = 0; way < graph.WayCount(); ++way) {
    const size_t tag = graph.TagOf(way);
    if (tag >= seen.size() || seen[tag] != 0) { return false; }
    const auto &lane = ways.Ways()[tag];
    const auto coordinates = WayPoints(lane, points);
    if (!MakesWay(lane) || !coordinates ||
        !graph.MatchesWay(way, *coordinates, WayProperties(lane, tag))) {
      return false;
    }
    seen[tag] = 1;
  }
  return true;
}

}
