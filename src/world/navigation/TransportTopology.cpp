#include "TransportTopology.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

namespace outshine::World {

namespace {

enum class WayTravel : uint8_t { Forward, Reverse, Both };

struct WaySemantics {
  bool TransportTagged = false;
  uint8_t Modes = 0;
  int32_t Layer = 0;
  bool Bridge = false;
  bool Tunnel = false;
  TransportAccess Access = TransportAccess::Public;
  WayTravel Travel = WayTravel::Both;
};

std::expected<std::optional<std::string_view>, TransportBuildErrorCode>
UniqueTag(std::span<const Data::OsmTag> tags, std::string_view key) {
  std::optional<std::string_view> value;
  for (const Data::OsmTag &tag : tags) {
    if (tag.Key != key) { continue; }
    if (value) { return std::unexpected(TransportBuildErrorCode::AmbiguousTag); }
    value = tag.Value;
  }
  return value;
}

template <size_t Count>
bool Contains(std::string_view value, const std::array<std::string_view, Count> &options) {
  return std::ranges::find(options, value) != options.end();
}

uint8_t HighwayModes(std::string_view kind) {
  constexpr std::array<std::string_view, 18> kMotor{"motorway",
                                                    "motorway_link",
                                                    "trunk",
                                                    "trunk_link",
                                                    "primary",
                                                    "primary_link",
                                                    "secondary",
                                                    "secondary_link",
                                                    "tertiary",
                                                    "tertiary_link",
                                                    "residential",
                                                    "unclassified",
                                                    "service",
                                                    "living_street",
                                                    "road",
                                                    "track",
                                                    "raceway",
                                                    "busway"};
  constexpr std::array<std::string_view, 5> kWalk{
      "footway", "pedestrian", "steps", "platform", "corridor"};
  if (Contains(kind, kMotor)) { return static_cast<uint8_t>(TransportMode::Motor); }
  if (Contains(kind, kWalk)) { return static_cast<uint8_t>(TransportMode::Walk); }
  if (kind == "cycleway") { return static_cast<uint8_t>(TransportMode::Cycle); }
  if (kind == "path" || kind == "bridleway") {
    return static_cast<uint8_t>(TransportMode::Walk) | static_cast<uint8_t>(TransportMode::Cycle);
  }
  return 0;
}

uint8_t RailwayModes(std::string_view kind) {
  constexpr std::array<std::string_view, 6> kRail{
      "rail", "light_rail", "subway", "tram", "narrow_gauge", "monorail"};
  return Contains(kind, kRail) ? static_cast<uint8_t>(TransportMode::Rail) : 0;
}

uint8_t WaterwayModes(std::string_view kind) {
  constexpr std::array<std::string_view, 2> kWater{"river", "canal"};
  return Contains(kind, kWater) ? static_cast<uint8_t>(TransportMode::Water) : 0;
}

std::expected<WayTravel, TransportBuildErrorCode>
ReadTravel(std::optional<std::string_view> oneWay,
           std::optional<std::string_view> highway,
           std::optional<std::string_view> junction) {
  if (!oneWay) {
    if (highway == "motorway" || highway == "motorway_link" || junction == "roundabout") {
      return WayTravel::Forward;
    }
    return WayTravel::Both;
  }
  if (*oneWay == "yes" || *oneWay == "1" || *oneWay == "true") { return WayTravel::Forward; }
  if (*oneWay == "-1" || *oneWay == "reverse") { return WayTravel::Reverse; }
  if (*oneWay == "no" || *oneWay == "0" || *oneWay == "false") { return WayTravel::Both; }
  return std::unexpected(TransportBuildErrorCode::InvalidOneway);
}

std::expected<int32_t, TransportBuildErrorCode> ReadLayer(std::optional<std::string_view> layer) {
  if (!layer) { return 0; }
  int32_t parsed = 0;
  const auto result = std::from_chars(layer->data(), layer->data() + layer->size(), parsed);
  if (result.ec != std::errc{} || result.ptr != layer->data() + layer->size()) {
    return std::unexpected(TransportBuildErrorCode::InvalidLayer);
  }
  return parsed;
}

bool Enabled(std::optional<std::string_view> value) {
  return value && *value != "no" && *value != "0" && *value != "false";
}

TransportAccess ReadAccess(std::optional<std::string_view> value) {
  if (value == "no") { return TransportAccess::Forbidden; }
  if (value == "private" || value == "destination" || value == "agricultural" ||
      value == "forestry") {
    return TransportAccess::Restricted;
  }
  return TransportAccess::Public;
}

std::expected<WaySemantics, TransportBuildErrorCode> DescribeWay(const Data::OsmWay &way) {
  const auto highway = UniqueTag(way.Tags, "highway");
  const auto railway = UniqueTag(way.Tags, "railway");
  const auto waterway = UniqueTag(way.Tags, "waterway");
  const auto route = UniqueTag(way.Tags, "route");
  const auto oneway = UniqueTag(way.Tags, "oneway");
  const auto junction = UniqueTag(way.Tags, "junction");
  const auto layer = UniqueTag(way.Tags, "layer");
  const auto bridge = UniqueTag(way.Tags, "bridge");
  const auto tunnel = UniqueTag(way.Tags, "tunnel");
  const auto access = UniqueTag(way.Tags, "access");
  if (!highway || !railway || !waterway || !route || !oneway || !junction || !layer || !bridge ||
      !tunnel || !access) {
    return std::unexpected(TransportBuildErrorCode::AmbiguousTag);
  }
  WaySemantics semantics;
  semantics.TransportTagged =
      highway->has_value() || railway->has_value() || waterway->has_value() || *route == "ferry";
  if (!semantics.TransportTagged) { return semantics; }
  semantics.Modes = (highway->has_value() ? HighwayModes(**highway) : 0) |
                    (railway->has_value() ? RailwayModes(**railway) : 0) |
                    (waterway->has_value() ? WaterwayModes(**waterway) : 0) |
                    (*route == "ferry" ? static_cast<uint8_t>(TransportMode::Water) : 0);
  const auto parsedLayer = ReadLayer(*layer);
  const auto travel = ReadTravel(*oneway, *highway, *junction);
  if (!parsedLayer) { return std::unexpected(parsedLayer.error()); }
  if (!travel) { return std::unexpected(travel.error()); }
  semantics.Layer = *parsedLayer;
  semantics.Travel = *travel;
  semantics.Bridge = Enabled(*bridge);
  semantics.Tunnel = Enabled(*tunnel);
  semantics.Access = ReadAccess(*access);
  return semantics;
}

}

std::expected<TransportTopology, TransportBuildError>
TransportTopology::Build(const Data::OsmElements &source) {
  if (const auto missing = source.FirstMissingReference()) {
    return std::unexpected(TransportBuildError{.Code = TransportBuildErrorCode::MissingSourceObject,
                                               .SourceId = missing->OwnerId});
  }
  TransportTopology built;
  built.SourceIdentity_ = source.SourceIdentity();
  built.Nodes_.reserve(source.Nodes().size());
  for (const Data::OsmNode &node : source.Nodes()) {
    built.Nodes_.push_back(TransportNode{.SourceNodeId = node.Id,
                                         .LatitudeDeg = node.LatitudeDeg,
                                         .LongitudeDeg = node.LongitudeDeg});
  }
  for (const Data::OsmWay &way : source.Ways()) {
    const auto described = DescribeWay(way);
    if (!described) {
      return std::unexpected(TransportBuildError{.Code = described.error(), .SourceId = way.Id});
    }
    if (!described->TransportTagged) { continue; }
    if (described->Modes == 0) { ++built.UnclassifiedWayCount_; }
    for (size_t segment = 1; segment < way.NodeIds.size(); ++segment) {
      if (segment - 1 > std::numeric_limits<uint32_t>::max() ||
          built.Edges_.size() > std::numeric_limits<uint32_t>::max() - 2u) {
        return std::unexpected(
            TransportBuildError{.Code = TransportBuildErrorCode::TooManyEdges, .SourceId = way.Id});
      }
      const uint64_t from = way.NodeIds[segment - 1];
      const uint64_t to = way.NodeIds[segment];
      if (from == to) {
        return std::unexpected(TransportBuildError{
            .Code = TransportBuildErrorCode::DegenerateSegment, .SourceId = way.Id});
      }
      const auto append = [&](EdgeDirection direction, uint64_t start, uint64_t end) {
        built.Edges_.push_back(
            TransportEdge{.Id = {.WayId = way.Id,
                                 .SegmentOrdinal = static_cast<uint32_t>(segment - 1),
                                 .Direction = direction},
                          .FromNodeId = start,
                          .ToNodeId = end,
                          .Modes = described->Modes,
                          .Layer = described->Layer,
                          .Bridge = described->Bridge,
                          .Tunnel = described->Tunnel,
                          .Access = described->Access});
      };
      if (described->Travel != WayTravel::Reverse) { append(EdgeDirection::Forward, from, to); }
      if (described->Travel != WayTravel::Forward) { append(EdgeDirection::Reverse, to, from); }
    }
  }
  std::ranges::sort(built.Edges_, {}, &TransportEdge::Id);
  built.Outgoing_.reserve(built.Edges_.size());
  for (size_t index = 0; index < built.Edges_.size(); ++index) {
    built.Outgoing_.push_back(OutgoingTransportEdge{.NodeId = built.Edges_[index].FromNodeId,
                                                    .EdgeIndex = static_cast<uint32_t>(index)});
  }
  std::ranges::sort(built.Outgoing_,
                    [&](const OutgoingTransportEdge &left, const OutgoingTransportEdge &right) {
                      if (left.NodeId != right.NodeId) { return left.NodeId < right.NodeId; }
                      return built.Edges_[left.EdgeIndex].Id < built.Edges_[right.EdgeIndex].Id;
                    });
  return built;
}

const TransportNode *TransportTopology::FindNode(uint64_t id) const noexcept {
  const auto found = std::lower_bound(
      Nodes_.begin(), Nodes_.end(), id, [](const TransportNode &node, uint64_t wanted) {
        return node.SourceNodeId < wanted;
      });
  return found != Nodes_.end() && found->SourceNodeId == id ? &*found : nullptr;
}

const TransportEdge *TransportTopology::FindEdge(TransportEdgeId id) const noexcept {
  const auto found = std::lower_bound(
      Edges_.begin(), Edges_.end(), id, [](const TransportEdge &edge, TransportEdgeId wanted) {
        return edge.Id < wanted;
      });
  return found != Edges_.end() && found->Id == id ? &*found : nullptr;
}

std::span<const OutgoingTransportEdge>
TransportTopology::OutgoingFrom(uint64_t nodeId) const noexcept {
  const auto first = std::lower_bound(
      Outgoing_.begin(),
      Outgoing_.end(),
      nodeId,
      [](const OutgoingTransportEdge &edge, uint64_t wanted) { return edge.NodeId < wanted; });
  const auto last = std::upper_bound(
      first, Outgoing_.end(), nodeId, [](uint64_t wanted, const OutgoingTransportEdge &edge) {
        return wanted < edge.NodeId;
      });
  return {first, last};
}

bool TransportTopology::CanContinue(const TransportEdge &from, const TransportEdge &to) noexcept {
  return from.ToNodeId == to.FromNodeId && (from.Modes & to.Modes) != 0 &&
         from.Access != TransportAccess::Forbidden && to.Access != TransportAccess::Forbidden;
}

}
