#include "GroupImpostorManifest.h"
#include "AssetPublication.h"
#include "GroupImpostorView.h"
#include "ImpostorViewFrame.h"
#include "Sha256.h"
#include "TangentFrame.h"
#include "Units.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace outshine {
namespace {
using Json = nlohmann::json;
constexpr double kBoundsM = 2e7;

bool Inside(double value, double limit) {
  return std::abs(value) <= limit;
}

std::optional<double> Number(const Json &value, double limit) {
  if (!value.is_number()) { return std::nullopt; }
  const auto number = value.get<double>();
  return Inside(number, limit) ? std::optional(number) : std::nullopt;
}

std::optional<Vec3> Vector(const Json &value) {
  if (!value.is_array() || value.size() != 3) { return std::nullopt; }
  Vec3 result;
  for (size_t axis = 0; axis < result.Axis.size(); ++axis) {
    const auto coordinate = Number(value[axis], kBoundsM);
    if (!coordinate) { return std::nullopt; }
    result[axis] = *coordinate;
  }
  return result;
}

std::optional<Data::TileId> Coverage(const Json &value) {
  if (!value.is_array() || value.size() != 3 ||
      !std::ranges::all_of(value, &Json::is_number_unsigned) ||
      value[0].get<uint64_t>() > Data::TileId::MaximumZoom) {
    return std::nullopt;
  }
  const auto zoom = value[0].get<unsigned>();
  const auto limit = uint64_t{1} << zoom;
  if (value[1].get<uint64_t>() >= limit || value[2].get<uint64_t>() >= limit) {
    return std::nullopt;
  }
  return Data::TileId{
      .Zoom = static_cast<int>(zoom), .X = value[1].get<uint32_t>(), .Y = value[2].get<uint32_t>()};
}

std::optional<LongitudeLatitude> Origin(const Json &value) {
  if (!value.is_array() || value.size() != 2) { return std::nullopt; }
  const auto lon = Number(value[0], kDegPerHalfTurn);
  const auto lat = Number(value[1], kDegPerHalfTurn * 0.5);
  if (!lon || !lat) { return std::nullopt; }
  return LongitudeLatitude{.LongitudeDeg = *lon, .LatitudeDeg = *lat};
}
}

std::optional<GroupImpostorManifest> ParseGroupImpostorManifest(const Json &root) {
  if (!root.contains("surface") || !root.contains("centre_m") || !root.contains("half_extent_m") ||
      !root.contains("card_side") || !root["card_side"].is_number_unsigned() ||
      !root.contains("card_views") || !root["card_views"].is_number_unsigned() ||
      !root.contains("directions") || !root["directions"].is_array()) {
    return std::nullopt;
  }
  const auto &surface = root["surface"];
  if (!surface.contains("coverage") || !surface.contains("origin_lon_lat") ||
      !surface.contains("revision") || !surface["revision"].is_string()) {
    return std::nullopt;
  }
  const auto &revision = surface["revision"].get_ref<const std::string &>();
  const auto tile = Coverage(surface["coverage"]);
  const auto origin = Origin(surface["origin_lon_lat"]);
  const auto centre = Vector(root["centre_m"]);
  const auto half = Number(root["half_extent_m"], kBoundsM);
  const auto side = root["card_side"].get<uint64_t>();
  const auto views = root["card_views"].get<uint64_t>();
  if (!tile || !origin || !centre || !half || *half <= 0 || !AssetProductKey::Parse(revision) ||
      side > GroupImpostorView::MaximumSide ||
      !GroupImpostorView::ValidSide(static_cast<uint32_t>(side)) || views < 2 ||
      views > GroupImpostorView::MaximumViews || views > root["directions"].size()) {
    return std::nullopt;
  }
  GroupImpostorManifest result{.Coverage = *tile,
                               .Origin = *origin,
                               .CentreM = *centre,
                               .HalfExtentM = *half,
                               .Side = static_cast<uint32_t>(side),
                               .SurfaceRevision = revision,
                               .Directions = {},
                               .CanopyCell = {}};
  if (const auto texel = root.find("maximum_projected_texel_px"); texel != root.end()) {
    constexpr double kLargestProjectedTexelPx = 2;
    const auto limit = Number(*texel, kLargestProjectedTexelPx);
    if (!limit || *limit < 1) { return std::nullopt; }
    result.MaximumTexelPx = *limit;
  }
  if (root.contains("canopy_cell")) {
    if (!root["canopy_cell"].is_string()) { return std::nullopt; }
    const auto &cell = root["canopy_cell"].get_ref<const std::string &>();
    if (!AssetProductKey::Parse(cell)) { return std::nullopt; }
    result.CanopyCell = cell;
  }
  result.Directions.reserve(views);
  for (size_t at = 0; at < views; ++at) {
    const auto direction = Vector(root["directions"][at]);
    if (!direction || !ImpostorViewFrame::From(*direction) ||
        std::ranges::find(result.Directions, *direction) != result.Directions.end()) {
      return std::nullopt;
    }
    result.Directions.push_back(*direction);
  }
  return result;
}

Box GroupImpostorEarthBounds(const GroupImpostorManifest &manifest) {
  const auto frame = TangentFrame::At(manifest.Origin);
  const Vec3 offset{{manifest.HalfExtentM, manifest.HalfExtentM, manifest.HalfExtentM}};
  const Box local{.Min = manifest.CentreM - offset, .Max = manifest.CentreM + offset};
  Box earth;
  for (unsigned corner = 0; corner < 8; ++corner) {
    const auto point = local.Corner(corner);
    earth.Cover(frame.ToEcefPosition({.EastM = point[0], .NorthM = -point[2], .UpM = point[1]}));
  }
  return earth;
}

std::string GroupImpostorRequestKey(std::string_view sourceKey) {
  return Sha256Hex(std::format("{}/{}", GroupImpostorManifestKind, sourceKey));
}

}
