#include "UrbanPrismAsset.h"
#include "Sha256.h"
#include "math/Box.h"
#include <scene/PrismGrid.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Generators::Osm {
namespace {
std::expected<Box, std::string> Coverage(const UrbanPrisms &product) {
  const auto inspected = InspectPrismGrid(product.Geometry);
  if (!inspected || inspected->Patches.empty()) {
    return std::unexpected("urban-prism package requires selected native patches");
  }
  Box result;
  for (size_t index = 0; index < inspected->Patches.size(); ++index) {
    const auto &patch = inspected->Patches[index];
    const Box local{.Min = {{0, patch.Lower, 0}}, .Max = {{patch.Width, patch.Upper, patch.Depth}}};
    const auto &frame = product.Placements[index].EcefFromLocal;
    Box world = local.Through(frame);
    for (size_t axis = 0; axis < 3; ++axis) {
      double scale = std::abs(frame.At(axis, 3));
      for (size_t column = 0; column < 3; ++column) {
        scale += std::abs(frame.At(axis, column)) *
                 std::max(std::abs(local.Min[column]), std::abs(local.Max[column]));
      }
      const double margin = 64 * std::numeric_limits<double>::epsilon() * std::max(1., scale);
      world.Min[axis] =
          std::nextafter(world.Min[axis] - margin, -std::numeric_limits<double>::infinity());
      world.Max[axis] =
          std::nextafter(world.Max[axis] + margin, std::numeric_limits<double>::infinity());
      if (!std::isfinite(world.Min[axis]) || !std::isfinite(world.Max[axis])) {
        return std::unexpected("urban-prism placement exceeds finite cache bounds");
      }
    }
    result.Cover(world);
  }
  return result;
}
}

std::expected<GeneratedAssetPackage, std::string> PackageUrbanPrisms(
    const UrbanPrisms &product, std::string_view request, UrbanPrismAssetOptions options) {
  if (request.size() != 64 ||
      request.find_first_not_of("0123456789abcdef") != std::string_view::npos) {
    return std::unexpected("urban-prism demand requires a SHA-256 request identity");
  }
  auto encoded = EncodeUrbanPrisms(product, options.BytesMost);
  if (!encoded) { return std::unexpected(encoded.error()); }
  const auto bounds = Coverage(product);
  if (!bounds) { return std::unexpected(bounds.error()); }
  const auto key = Sha256Hex(std::string(request) + Sha256Hex(encoded->data(), encoded->size()));
  return GeneratedAssetPackage{.Records = {{.Key = key,
                                            .Kind = "urban-prisms",
                                            .Bounds = *bounds,
                                            .Package = {},
                                            .ByteCount = encoded->size(),
                                            .Level = options.Level,
                                            .Parent = {},
                                            .RequestKey = std::string(request)}},
                               .Bytes = std::move(*encoded)};
}
}
