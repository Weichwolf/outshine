#include "OsmWaySemantics.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <system_error>

namespace outshine::World {

namespace {

constexpr double kMotorwayWidthM = 7.5;
constexpr double kTrunkArterialWidthM = 7.0;
constexpr double kLocalMixedWidthM = 6.0;
constexpr double kServiceWidthM = 4.5;
constexpr double kTrackWidthM = 3.0;
constexpr double kRacewayWidthM = 12.0;
constexpr double kWalkwayWidthM = 2.0;
constexpr double kCyclewayWidthM = 2.5;
constexpr double kRailwayWidthM = 3.5;
constexpr double kWaterWidthM = 8.0;
constexpr double kFeetToMetres = 0.3048;
constexpr double kMinimumWidthM = 0.3;
constexpr double kMaximumWidthM = 1000.0;
constexpr double kMotorLaneWidthM = 3.25;

struct DirectionTags {
  std::optional<std::string_view> OneWay;
  std::optional<std::string_view> Highway;
  std::optional<std::string_view> Junction;
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

std::expected<OsmWayTravel, TransportBuildErrorCode> ReadTravel(const DirectionTags &tags) {
  if (!tags.OneWay) {
    if (tags.Highway == "motorway" || tags.Highway == "motorway_link" ||
        tags.Junction == "roundabout") {
      return OsmWayTravel::Forward;
    }
    return OsmWayTravel::Both;
  }
  if (*tags.OneWay == "yes" || *tags.OneWay == "1" || *tags.OneWay == "true") {
    return OsmWayTravel::Forward;
  }
  if (*tags.OneWay == "-1" || *tags.OneWay == "reverse") { return OsmWayTravel::Reverse; }
  if (*tags.OneWay == "no" || *tags.OneWay == "0" || *tags.OneWay == "false") {
    return OsmWayTravel::Both;
  }
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

constexpr TransportFacility HighwayFacility(std::string_view kind) {
  if (kind == "motorway" || kind == "motorway_link") { return TransportFacility::Motorway; }
  if (kind == "trunk" || kind == "trunk_link") { return TransportFacility::Trunk; }
  if (kind == "primary" || kind == "primary_link" || kind == "secondary" ||
      kind == "secondary_link" || kind == "tertiary" || kind == "tertiary_link") {
    return TransportFacility::Arterial;
  }
  if (kind == "residential" || kind == "unclassified" || kind == "living_street" ||
      kind == "road") {
    return TransportFacility::LocalStreet;
  }
  if (kind == "service" || kind == "busway") { return TransportFacility::ServiceRoad; }
  if (kind == "track") { return TransportFacility::Track; }
  if (kind == "raceway") { return TransportFacility::Raceway; }
  if (kind == "footway" || kind == "pedestrian" || kind == "steps" || kind == "platform" ||
      kind == "corridor" || kind == "path" || kind == "bridleway") {
    return TransportFacility::Walkway;
  }
  if (kind == "cycleway") { return TransportFacility::Cycleway; }
  return TransportFacility::Unknown;
}

constexpr double DefaultWidthM(TransportFacility facility) {
  switch (facility) {
    case TransportFacility::Motorway: return kMotorwayWidthM;
    case TransportFacility::Trunk:
    case TransportFacility::Arterial: return kTrunkArterialWidthM;
    case TransportFacility::LocalStreet:
    case TransportFacility::Mixed: return kLocalMixedWidthM;
    case TransportFacility::ServiceRoad: return kServiceWidthM;
    case TransportFacility::Track: return kTrackWidthM;
    case TransportFacility::Raceway: return kRacewayWidthM;
    case TransportFacility::Walkway: return kWalkwayWidthM;
    case TransportFacility::Cycleway: return kCyclewayWidthM;
    case TransportFacility::Railway: return kRailwayWidthM;
    case TransportFacility::Waterway:
    case TransportFacility::Ferry: return kWaterWidthM;
    case TransportFacility::Unknown: return 0.0;
  }
  return 0.0;
}

TransportSurface SurfaceOf(std::optional<std::string_view> surface, TransportFacility facility) {
  if (surface) {
    if (*surface == "asphalt") { return TransportSurface::Asphalt; }
    if (*surface == "concrete" || *surface == "concrete:lanes" || *surface == "concrete:plates") {
      return TransportSurface::Concrete;
    }
    if (*surface == "paved" || *surface == "paving_stones" || *surface == "sett" ||
        *surface == "cobblestone") {
      return TransportSurface::Paved;
    }
    if (*surface == "gravel" || *surface == "fine_gravel" || *surface == "pebblestone" ||
        *surface == "compacted") {
      return TransportSurface::Gravel;
    }
    if (*surface == "dirt" || *surface == "earth" || *surface == "ground" || *surface == "grass" ||
        *surface == "sand" || *surface == "unpaved") {
      return TransportSurface::Earth;
    }
    return TransportSurface::Unknown;
  }
  if (facility == TransportFacility::Railway) { return TransportSurface::Rail; }
  if (facility == TransportFacility::Waterway || facility == TransportFacility::Ferry) {
    return TransportSurface::Water;
  }
  if (facility == TransportFacility::Track) { return TransportSurface::Earth; }
  if (facility == TransportFacility::Walkway || facility == TransportFacility::Cycleway) {
    return TransportSurface::Paved;
  }
  return facility == TransportFacility::Unknown ? TransportSurface::Unknown
                                                : TransportSurface::Asphalt;
}

std::expected<double, TransportBuildErrorCode> ReadWidthM(std::optional<std::string_view> width) {
  if (!width) { return 0.0; }
  std::string_view text = *width;
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) { text.remove_prefix(1); }
  double measured = 0.0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), measured);
  if (parsed.ec != std::errc{}) { return std::unexpected(TransportBuildErrorCode::InvalidWidth); }
  text.remove_prefix(static_cast<size_t>(parsed.ptr - text.data()));
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) { text.remove_prefix(1); }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) { text.remove_suffix(1); }
  if (text == "ft" || text == "feet" || text == "'") {
    measured *= kFeetToMetres;
  } else if (!text.empty() && text != "m") {
    return std::unexpected(TransportBuildErrorCode::InvalidWidth);
  }
  if (!std::isfinite(measured) || measured < kMinimumWidthM || measured > kMaximumWidthM) {
    return std::unexpected(TransportBuildErrorCode::InvalidWidth);
  }
  return measured;
}

std::expected<uint8_t, TransportBuildErrorCode>
ReadLaneCount(std::optional<std::string_view> lanes) {
  if (!lanes) { return uint8_t{0}; }
  uint32_t count = 0;
  const auto parsed = std::from_chars(lanes->data(), lanes->data() + lanes->size(), count);
  if (parsed.ec != std::errc{} || parsed.ptr != lanes->data() + lanes->size() || count == 0 ||
      count > 32) {
    return std::unexpected(TransportBuildErrorCode::InvalidLaneCount);
  }
  return static_cast<uint8_t>(count);
}

}

std::expected<OsmWaySemantics, TransportBuildErrorCode> DescribeOsmWay(const Data::OsmWay &way) {
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
  const auto width = UniqueTag(way.Tags, "width");
  const auto lanes = UniqueTag(way.Tags, "lanes");
  const auto surface = UniqueTag(way.Tags, "surface");
  if (!highway || !railway || !waterway || !route || !oneway || !junction || !layer || !bridge ||
      !tunnel || !access || !width || !lanes || !surface) {
    return std::unexpected(TransportBuildErrorCode::AmbiguousTag);
  }
  OsmWaySemantics semantics;
  semantics.TransportTagged =
      highway->has_value() || railway->has_value() || waterway->has_value() || *route == "ferry";
  if (!semantics.TransportTagged) { return semantics; }
  unsigned modeBits = 0;
  if (highway->has_value()) { modeBits |= HighwayModes(**highway); }
  if (railway->has_value()) { modeBits |= RailwayModes(**railway); }
  if (waterway->has_value()) { modeBits |= WaterwayModes(**waterway); }
  if (*route == "ferry") { modeBits |= static_cast<unsigned>(TransportMode::Water); }
  semantics.Modes = static_cast<uint8_t>(modeBits);
  const unsigned taggedKinds =
      static_cast<unsigned>(highway->has_value()) + static_cast<unsigned>(railway->has_value()) +
      static_cast<unsigned>(waterway->has_value()) + static_cast<unsigned>(*route == "ferry");
  if (taggedKinds > 1) {
    semantics.Facility = TransportFacility::Mixed;
  } else if (highway->has_value()) {
    semantics.Facility = HighwayFacility(**highway);
  } else if (railway->has_value() && RailwayModes(**railway) != 0) {
    semantics.Facility = TransportFacility::Railway;
  } else if (waterway->has_value() && WaterwayModes(**waterway) != 0) {
    semantics.Facility = TransportFacility::Waterway;
  } else if (*route == "ferry") {
    semantics.Facility = TransportFacility::Ferry;
  }
  const auto parsedLayer = ReadLayer(*layer);
  const auto travel = ReadTravel({.OneWay = *oneway, .Highway = *highway, .Junction = *junction});
  const auto parsedWidth = ReadWidthM(*width);
  const auto parsedLanes = ReadLaneCount(*lanes);
  if (!parsedLayer) { return std::unexpected(parsedLayer.error()); }
  if (!travel) { return std::unexpected(travel.error()); }
  if (!parsedWidth) { return std::unexpected(parsedWidth.error()); }
  if (!parsedLanes) { return std::unexpected(parsedLanes.error()); }
  semantics.LaneCount = *parsedLanes;
  semantics.WidthM = DefaultWidthM(semantics.Facility);
  if (*parsedLanes != 0 && (semantics.Modes & static_cast<uint8_t>(TransportMode::Motor)) != 0) {
    const double laneWidthM = static_cast<double>(*parsedLanes) * kMotorLaneWidthM;
    semantics.WidthM = semantics.Facility == TransportFacility::Raceway
                           ? std::max(semantics.WidthM, laneWidthM)
                           : laneWidthM;
  }
  if (*parsedWidth != 0.0) { semantics.WidthM = *parsedWidth; }
  semantics.Surface = SurfaceOf(*surface, semantics.Facility);
  semantics.Layer = *parsedLayer;
  semantics.Travel = *travel;
  semantics.Bridge = Enabled(*bridge);
  semantics.Tunnel = Enabled(*tunnel);
  semantics.Access = ReadAccess(*access);
  return semantics;
}

}
