#include "UrbanAreaAsset.h"
#include "Geodesy.h"
#include "Sha256.h"
#include "Wgs84.h"
#include "Box.h"
#include "Vec3.h"
#include "Units.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <numbers>
#include <limits>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Generators::Osm {
namespace {
Box Coverage(const UrbanArea &area) {
  const auto &tile = area.Inputs.front().Identity.Tile;
  const double span = std::ldexp(Data::kMercatorGirthM, -tile.Zoom);
  const double halfWorld = std::numbers::pi * kWgs84A;
  const double halo = area.Recipe.ProjectedPitchM;
  const double west = (tile.X * span - halfWorld - halo) / kWgs84A;
  const double east = ((static_cast<double>(tile.X) + 1) * span - halfWorld + halo) / kWgs84A;
  const double north = std::atan(std::sinh((halfWorld - tile.Y * span + halo) / kWgs84A));
  const double south =
      std::atan(std::sinh((halfWorld - (static_cast<double>(tile.Y) + 1) * span - halo) / kWgs84A));
  constexpr double degrees = 180 / std::numbers::pi;
  Vec3 centre;
  GeoToEcef({.LongitudeDeg = std::midpoint(west, east) * degrees,
             .LatitudeDeg = std::midpoint(south, north) * degrees,
             .HeightM = 0},
            centre);
  const double derivative = kWgs84A / (1 - kWgs84F);
  const double radius = derivative * ((east - west) + (north - south)) * .5;
  Box result;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double margin =
        64 * std::numeric_limits<double>::epsilon() * (std::abs(centre[axis]) + radius + kWgs84A);
    result.Min[axis] =
        std::nextafter(centre[axis] - radius - margin, -std::numeric_limits<double>::infinity());
    result.Max[axis] =
        std::nextafter(centre[axis] + radius + margin, std::numeric_limits<double>::infinity());
  }
  return result;
}
}

std::expected<GeneratedAssetPackage, std::string>
PackageUrbanArea(const UrbanArea &area, std::string_view request, size_t bytesMost) {
  if (request.size() != 64 ||
      request.find_first_not_of("0123456789abcdef") != std::string_view::npos) {
    return std::unexpected("urban-area demand requires a SHA-256 request identity");
  }
  auto encoded = EncodeUrbanArea(area, bytesMost);
  if (!encoded) { return std::unexpected(encoded.error()); }
  const auto key = Sha256Hex(std::string(request) + Sha256Hex(encoded->data(), encoded->size()));
  return GeneratedAssetPackage{
      .Records = {{.Key = key,
                   .Kind = "urban-area",
                   .Bounds = Coverage(area),
                   .Package = {},
                   .ByteCount = encoded->size(),
                   .Level = static_cast<uint32_t>(area.Inputs.front().Identity.Tile.Zoom),
                   .Parent = {},
                   .RequestKey = std::string(request)}},
      .Bytes = std::move(*encoded)};
}
}
