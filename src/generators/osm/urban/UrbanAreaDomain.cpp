#include "UrbanAreaDomain.h"
#include "Wgs84.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm::UrbanDetail {
namespace {
constexpr double kContextGuardM = 512;
constexpr int kBufferSegments = 8;

struct FeatureClass {
  std::string_view Layer, Kind;
  bool Tunnel = false;
};

struct ClassifiedGeometry {
  Spatial::GeosGeometry Geometry;
  UrbanSourceRole Role = UrbanSourceRole::Unclassified;
};

struct Domains {
  std::vector<Spatial::GeosGeometry> Urban, Open;
};

bool Urban(std::string_view kind) {
  constexpr std::array kinds{"residential",
                             "commercial",
                             "industrial",
                             "retail",
                             "suburb",
                             "quarter",
                             "neighbourhood",
                             "school",
                             "university",
                             "hospital",
                             "college",
                             "kindergarten",
                             "library",
                             "military",
                             "garages",
                             "bus_station"};
  return std::ranges::contains(kinds, kind);
}

bool Open(std::string_view kind) {
  constexpr std::array kinds{
      "park", "cemetery", "pitch", "playground", "track", "stadium", "parking", "railway"};
  return std::ranges::contains(kinds, kind);
}

using WidthEntry = std::pair<std::string_view, double>;

double Width(FeatureClass kind) {
  constexpr std::string_view suffix = "_construction";
  if (kind.Kind.ends_with(suffix)) { kind.Kind.remove_suffix(suffix.size()); }
  constexpr std::array roads{WidthEntry{"motorway", 14},
                             WidthEntry{"trunk", 12},
                             WidthEntry{"primary", 9},
                             WidthEntry{"secondary", 7},
                             WidthEntry{"tertiary", 6},
                             WidthEntry{"rail", 4}};
  constexpr std::array waterways{WidthEntry{"river", 5},
                                 WidthEntry{"stream", 1.5},
                                 WidthEntry{"canal", 4},
                                 WidthEntry{"drain", 1},
                                 WidthEntry{"ditch", 1}};
  constexpr std::array aeroways{WidthEntry{"runway", 25}, WidthEntry{"taxiway", 8}};
  std::span<const WidthEntry> choices;
  if (kind.Layer == "streets") { choices = roads; }
  if (kind.Layer == "water_lines") { choices = waterways; }
  if (kind.Layer == "aeroway") { choices = aeroways; }
  const auto found = std::ranges::find(choices, kind.Kind, &WidthEntry::first);
  return found == choices.end() ? 0 : found->second;
}

std::expected<UrbanSourceRole, std::string>
Classify(const Domain &domain, FeatureClass kind, Spatial::GeosGeometry &geometry) {
  const int type = GEOSGeomTypeId_r(domain.Context, geometry.get());
  if (type == GEOS_POLYGON || type == GEOS_MULTIPOLYGON) {
    if (kind.Layer == "sites" && Urban(kind.Kind)) { return UrbanSourceRole::Urban; }
    if (kind.Layer == "water_polygons" || kind.Layer == "land" || kind.Layer == "aeroway" ||
        kind.Layer == "park" || Open(kind.Kind)) {
      return UrbanSourceRole::OpenSpace;
    }
  }
  if (type == GEOS_LINESTRING || type == GEOS_MULTILINESTRING) {
    if (kind.Tunnel) { return UrbanSourceRole::Tunnel; }
    const double width = Width(kind);
    if (width != 0) {
      geometry = domain.Own(
          GEOSBuffer_r(domain.Context, geometry.get(), width / domain.Scale, kBufferSegments));
      if (!geometry) { return std::unexpected("urban corridor buffer failed"); }
      return UrbanSourceRole::Corridor;
    }
  }
  return UrbanSourceRole::Unclassified;
}

std::expected<ClassifiedGeometry, std::string> Inspect(const Domain &domain,
                                                       const OsmField &field,
                                                       const OsmField::Feature &feature,
                                                       const GEOSGeometry &guard) {
  ClassifiedGeometry result{.Geometry = domain.Own(nullptr)};
  if (feature.Type != 2 && feature.Type != 3) { return result; }
  result.Geometry = SourceGeometry(domain, field, feature);
  if (!result.Geometry) { return std::unexpected("urban source geometry allocation failed"); }
  const char valid = GEOSisValid_r(domain.Context, result.Geometry.get());
  if (valid == 2) { return std::unexpected("urban source validity query failed"); }
  if (valid == 0) {
    result.Role = UrbanSourceRole::Invalid;
    return result;
  }
  result.Geometry = domain.Own(GEOSIntersection_r(domain.Context, result.Geometry.get(), &guard));
  if (!result.Geometry) { return std::unexpected("urban context clipping failed"); }
  const char empty = GEOSisEmpty_r(domain.Context, result.Geometry.get());
  if (empty == 2) { return std::unexpected("urban source empty query failed"); }
  if (empty == 1) {
    result.Role = UrbanSourceRole::Outside;
    return result;
  }
  const FeatureClass kind{.Layer = field.LayerName(feature.Layer),
                          .Kind = field.Str(feature, "kind"),
                          .Tunnel = field.Num(feature, "tunnel", 0) != 0};
  const auto role = Classify(domain, kind, result.Geometry);
  if (!role) { return std::unexpected(role.error()); }
  result.Role = *role;
  return result;
}

std::expected<Domains, std::string> CollectInputs(Domain &domain,
                                                  std::span<const OsmField *const> inputs,
                                                  const GEOSGeometry &guard,
                                                  const UrbanAreaLimits &limits) {
  Domains result;
  size_t vertices = 0;
  for (size_t input = 0; input < inputs.size(); ++input) {
    const auto *field = inputs[input];
    if (field == nullptr || field->Tiles().size() != 1 ||
        field->Points().size() / 2 > limits.SourceVerticesMost - vertices ||
        field->Features().size() > limits.FeaturesMost - domain.Sources.size()) {
      return std::unexpected("urban source budget or single-tile contract exceeded");
    }
    vertices += field->Points().size() / 2;
    for (size_t i = 0; i < field->Features().size(); ++i) {
      if (Expired(limits)) { return std::unexpected("urban generation cancelled or expired"); }
      const auto &feature = field->Features()[i];
      if (input != 0 && field->LayerName(feature.Layer) != "sites") {
        domain.Sources.push_back({.Input = static_cast<uint32_t>(input),
                                  .Feature = static_cast<uint32_t>(i),
                                  .Role = UrbanSourceRole::OutsideScope,
                                  .ProviderFeatureId = feature.ProviderFeatureId});
        continue;
      }
      auto inspected = Inspect(domain, *field, feature, guard);
      if (!inspected) { return std::unexpected(inspected.error()); }
      domain.Sources.push_back({.Input = static_cast<uint32_t>(input),
                                .Feature = static_cast<uint32_t>(i),
                                .Role = inspected->Role,
                                .ProviderFeatureId = feature.ProviderFeatureId});
      if (inspected->Role == UrbanSourceRole::Urban) {
        result.Urban.push_back(std::move(inspected->Geometry));
      }
      if (inspected->Role == UrbanSourceRole::OpenSpace ||
          inspected->Role == UrbanSourceRole::Corridor) {
        result.Open.push_back(std::move(inspected->Geometry));
      }
    }
  }
  return result;
}
}

Domain::~Domain() {
  if (Prepared != nullptr) { GEOSPreparedGeom_destroy_r(Context, Prepared); }
  Allowed.reset();
  if (Context != nullptr) { GEOS_finish_r(Context); }
}

bool Expired(const UrbanAreaLimits &limits) {
  return limits.Stop.stop_requested() || std::chrono::steady_clock::now() >= limits.Deadline;
}

OwnerFrame FrameFor(const OsmField::Tile &owner) {
  const double span = 2 * std::numbers::pi * kWgs84A / std::ldexp(1.0, owner.Z);
  const double left = owner.X * span - std::numbers::pi * kWgs84A;
  const double top = std::numbers::pi * kWgs84A - owner.Y * span;
  return {.Bounds = {left, top - span, left + span, top},
          .Scale = std::cos(std::atan(std::sinh((top - span / 2) / kWgs84A)))};
}

std::expected<bool, std::string> Domain::Covers(std::array<double, 4> bounds) const {
  const auto box =
      Own(GEOSGeom_createRectangle_r(Context, bounds[0], bounds[1], bounds[2], bounds[3]));
  if (!box) { return std::unexpected("urban footprint allocation failed"); }
  const char covered = GEOSPreparedCovers_r(Context, Prepared, box.get());
  if (covered == 2) { return std::unexpected("urban coverage query failed"); }
  return covered == 1;
}

std::expected<std::unique_ptr<Domain>, std::string> BuildDomain(
    std::span<const OsmField *const> inputs, OwnerFrame frame, const UrbanAreaLimits &limits) {
  if (inputs.empty() || inputs.size() > 16) {
    return std::unexpected("bounded urban context tiles required");
  }
  if (Expired(limits)) { return std::unexpected("urban generation cancelled or expired"); }
  auto domain = std::make_unique<Domain>();
  if (domain->Context == nullptr) {
    return std::unexpected("urban geometry context allocation failed");
  }
  domain->Bounds = frame.Bounds;
  domain->Scale = frame.Scale;
  const auto guard = domain->Own(GEOSGeom_createRectangle_r(domain->Context,
                                                            frame.Bounds[0] - kContextGuardM,
                                                            frame.Bounds[1] - kContextGuardM,
                                                            frame.Bounds[2] + kContextGuardM,
                                                            frame.Bounds[3] + kContextGuardM));
  if (!guard) { return std::unexpected("urban context bounds allocation failed"); }
  auto parts = CollectInputs(*domain, inputs, *guard, limits);
  if (!parts) { return std::unexpected(parts.error()); }
  domain->HasUrbanEvidence = !parts->Urban.empty();
  const auto urbanParts = Collect(*domain, std::move(parts->Urban), GEOS_GEOMETRYCOLLECTION);
  const auto openParts = Collect(*domain, std::move(parts->Open), GEOS_GEOMETRYCOLLECTION);
  if (!urbanParts || !openParts) { return std::unexpected("urban source collection failed"); }
  const auto urbanUnion = domain->Own(GEOSUnaryUnion_r(domain->Context, urbanParts.get()));
  const auto openUnion = domain->Own(GEOSUnaryUnion_r(domain->Context, openParts.get()));
  if (!urbanUnion || !openUnion) { return std::unexpected("urban source union failed"); }
  if (Expired(limits)) { return std::unexpected("urban generation cancelled or expired"); }
  domain->Allowed =
      domain->Own(GEOSDifference_r(domain->Context, urbanUnion.get(), openUnion.get()));
  if (!domain->Allowed || GEOSisValid_r(domain->Context, domain->Allowed.get()) != 1) {
    return std::unexpected("urban permitted domain failed validation");
  }
  domain->Prepared = GEOSPrepare_r(domain->Context, domain->Allowed.get());
  if (domain->Prepared == nullptr) { return std::unexpected("urban prepared domain failed"); }
  return domain;
}
}
