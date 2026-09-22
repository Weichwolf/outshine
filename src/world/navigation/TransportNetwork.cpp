#include "TransportNetwork.h"

#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <ratio>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "Earth.h"
#include "OsmField.h"
#include "StreetField.h"

namespace outshine::World {
namespace Says {
constexpr auto kInvalidTransportPointRange =
    "transport way point range exceeds the supplied coordinate stream";
}

TransportNetwork::Built TransportNetwork::BuildOneShot(const Ground::GroundStack &stack) {
  Built made;
  const Ground::OsmField *const vectors = stack.Vectors();
  if (vectors == nullptr) { return made; }
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
  if (const auto laid = LayWays(stack.Ways(), vectors->Points(), *graph); !laid) {
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
  made.Elevated =
      graph->Elevate([&stack](LongitudeLatitude at) { return stack.Ground().At(at).AslM(); });
  made.ElevateMs = phaseMs();
  made.Graph = std::move(graph);
  return made;
}

std::expected<void, std::string_view> TransportNetwork::LayWays(const Ground::StreetField &ways,
                                                                std::span<const double> points,
                                                                Path::Network &graph) {
  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const Ground::StreetField::Way &lane = ways.Ways()[at];
    if (lane.Form != Ground::StreetField::Shape::Ribbon || lane.PointCount < 2) { continue; }
    const size_t first = static_cast<size_t>(lane.FirstPoint) * 2;
    if (first > points.size() || lane.PointCount > (points.size() - first) / 2) {
      return std::unexpected(Says::kInvalidTransportPointRange);
    }
    const auto laid = graph.Lay(points.subspan(first, static_cast<size_t>(lane.PointCount) * 2),
                                Path::WayClass{.HalfWidthM = static_cast<double>(lane.HalfWidthM),
                                               .MaxGradient = 0.0,
                                               .MinRadiusM = 0.0,
                                               .Friction = 0.0,
                                               .SpeedMps = static_cast<double>(lane.SpeedMps),
                                               .Lanes = lane.Lanes,
                                               .Priority = lane.Priority,
                                               .Oneway = lane.Oneway,
                                               .Sealed = lane.Sealed,
                                               .Spans = lane.Bridge,
                                               .Tag = at});
    if (!laid) { return laid; }
  }
  return {};
}

}
