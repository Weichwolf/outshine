#include "UrbanPrismAsset.h"
#include "Sha256.h"
#include "BinaryValueArchive.h"
#include "Box.h"
#include <scene/PrismGrid.h>
#include <algorithm>
#include <array>
#include <span>
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

std::expected<std::string, std::string>
UrbanPrismRequestKey(std::string_view areaAssetKey,
                     const std::array<uint8_t, 32> &terrainIdentity,
                     std::span<const UrbanPatchRange> patches,
                     uint32_t level) {
  constexpr size_t patchesMost = kUrbanPrismBytesMost / sizeof(PrismSpan);
  if (areaAssetKey.size() != 64 ||
      areaAssetKey.find_first_not_of("0123456789abcdef") != std::string_view::npos ||
      patches.empty() || patches.size() > patchesMost) {
    return std::unexpected("urban-prism request requires a source asset and bounded selection");
  }
  constexpr size_t headerBytesMost = 160;
  BinaryValueWriter writer(headerBytesMost + patches.size() * sizeof(uint32_t) * 4);
  if (!writer.Text("urban-prisms-generator-1") || !writer(kUrbanPrismFormat) ||
      !writer.Text(areaAssetKey) ||
      !writer(terrainIdentity, level, static_cast<uint32_t>(patches.size()))) {
    return std::unexpected("could not encode urban-prism request header");
  }
  uint64_t samples = 0;
  for (const auto &patch : patches) {
    const uint64_t count = uint64_t{patch.Columns} * patch.Rows;
    if (patch.Columns == 0 || patch.Rows == 0 || count > patchesMost - samples ||
        patch.Column > std::numeric_limits<uint32_t>::max() - patch.Columns ||
        patch.Row > std::numeric_limits<uint32_t>::max() - patch.Rows ||
        !writer(patch.Column, patch.Row, patch.Columns, patch.Rows)) {
      return std::unexpected("urban-prism request has invalid or excessive sample ranges");
    }
    samples += count;
  }
  return Sha256Hex(writer.Out.Bytes().data(), writer.Out.Bytes().size());
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
