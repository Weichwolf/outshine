#include "GeometryAsset.h"
#include "GeometryProductCodec.h"
#include "ClusteredGeometryCodec.h"
#include "AssetPublication.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <limits>
#include <vector>

namespace outshine {
namespace {
constexpr uint64_t kFormat = 0x3154455353414547ULL;
constexpr uint64_t kCoverageFormat = 0x3254455353414547ULL;
constexpr uint64_t kClusteredFormat = 0x3354455353414547ULL;

std::string DescriptionKey(std::string_view key) {
  const auto recipe = "geometry-asset-info-1/" + std::string(key);
  return Sha256Hex(recipe.data(), recipe.size());
}

bool Valid(const GeometryAsset::Description &description) {
  const auto finite = [](double value) { return std::isfinite(value); };
  const auto &bounds = description.Bounds;
  const auto &world = description.WorldFromLocal;
  if (!std::ranges::all_of(bounds.Min, finite) || !std::ranges::all_of(bounds.Max, finite) ||
      !std::ranges::all_of(world.Column, finite)) {
    return false;
  }
  for (size_t axis = 0; axis < 3; ++axis) {
    if (bounds.Min[axis] > bounds.Max[axis] || world.At(3, axis) != 0) { return false; }
  }
  const auto worldBounds = bounds.Through(world);
  return world.At(3, 3) == 1 && std::ranges::all_of(worldBounds.Min, finite) &&
         std::ranges::all_of(worldBounds.Max, finite);
}

bool ReadCoverage(BinaryValueReader &in, GeometryAsset::Description &description) {
  std::string revision;
  uint64_t sources = 0;
  uint32_t count = 0;
  if (!in.Text(revision) || !in(sources, count) ||
      count > in.In.Remaining() / (2 * sizeof(uint64_t))) {
    return false;
  }
  std::vector<SourceCoverage::Range> ranges(count);
  for (auto &range : ranges) {
    if (!in(range.First, range.Count)) { return false; }
  }
  auto coverage = SourceCoverage::FromRanges(std::move(revision), sources, ranges);
  if (!coverage) { return false; }
  description.Coverage = std::move(*coverage);
  return true;
}

bool WriteDescription(BinaryValueWriter &out, const GeometryAsset::Description &description) {
  uint64_t format = description.Coverage ? kCoverageFormat : kFormat;
  if (description.ClusteredParts != 0) { format = kClusteredFormat; }
  if (!out(format,
           description.Bounds.Min.Axis,
           description.Bounds.Max.Axis,
           description.WorldFromLocal.Column)) {
    return false;
  }
  if (description.ClusteredParts != 0 &&
      !out(description.ClusteredParts, description.Coverage.has_value())) {
    return false;
  }
  if (!description.Coverage) { return true; }
  const auto &coverage = *description.Coverage;
  if (coverage.Ranges().size() > std::numeric_limits<uint32_t>::max() ||
      !out.Text(coverage.Revision()) ||
      !out(coverage.SourceCount(), static_cast<uint32_t>(coverage.Ranges().size()))) {
    return false;
  }
  for (const auto &range : coverage.Ranges()) {
    if (!out(range.First, range.Count)) { return false; }
  }
  return true;
}

bool Contains(const Geometry &geometry, const Box &bounds) {
  for (int part = 0; part < geometry.parts(); ++part) {
    const auto vertices = geometry.positionsOf(part);
    const auto &placement = geometry.placementOf(part);
    for (size_t at = 0; at < vertices.size(); at += 3) {
      const Vec3 point{{vertices[at], vertices[at + 1], vertices[at + 2]}};
      if (!bounds.Holds(placement.TransformPoint(point))) { return false; }
    }
  }
  return true;
}

std::optional<ClusteredGeometryProduct>
ReadPayload(std::span<const uint8_t> bytes, size_t bytesMost, bool clustered) {
  if (clustered) { return DecodeClusteredGeometry(bytes, bytesMost); }
  auto geometry = DecodeGeometryProduct(bytes, bytesMost);
  if (!geometry) { return std::nullopt; }
  return ClusteredGeometryProduct{.Mesh = std::move(*geometry), .Clusters = {}};
}

}

size_t GeometryAsset::Product::HeapBytes() const noexcept {
  size_t bytes = Mesh.storageBytes() + Clusters.capacity() * sizeof(std::vector<DagCluster>);
  for (const auto &part : Clusters) { bytes += part.capacity() * sizeof(DagCluster); }
  return bytes + (Layout.Coverage ? Layout.Coverage->CapacityBytes() : 0);
}

std::expected<void, AssetCacheError> GeometryAsset::Publish(AssetCache &cache,
                                                            const std::string &key,
                                                            const std::string &parent,
                                                            const Product &product,
                                                            size_t bytesMost) {
  return Publish(cache, key, parent, product.Layout, product.Mesh, bytesMost, product.Clusters);
}

std::expected<std::optional<GeometryAsset::Description>, AssetCacheError> GeometryAsset::Describe(
    const AssetCache &cache, std::string_view key, std::string_view parent, size_t bytesMost) {
  if (!AssetProductKey::Parse(key) || (!parent.empty() && !AssetProductKey::Parse(parent))) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  auto loaded = cache.Load(DescriptionKey(key), bytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) { return std::nullopt; }
  const auto &record = (**loaded).Record();
  BinaryValueReader in((**loaded).Bytes());
  uint64_t format = 0;
  bool hasCoverage = false;
  Description description;
  if (record.Kind != "native-geometry-info" || record.Parent != parent ||
      !in(format,
          description.Bounds.Min.Axis,
          description.Bounds.Max.Axis,
          description.WorldFromLocal.Column) ||
      (format != kFormat && format != kCoverageFormat && format != kClusteredFormat) ||
      (format == kClusteredFormat &&
       (!in(description.ClusteredParts, hasCoverage) || description.ClusteredParts == 0)) ||
      ((format == kCoverageFormat || hasCoverage) && !ReadCoverage(in, description)) ||
      in.In.Remaining() != 0 || !Valid(description) ||
      description.Bounds.Through(description.WorldFromLocal) != record.Bounds) {
    return std::unexpected(AssetCacheError::Storage);
  }
  return description;
}

std::expected<std::optional<GeometryAsset::Product>, AssetCacheError> GeometryAsset::Load(
    const AssetCache &cache, std::string_view key, std::string_view parent, size_t bytesMost) {
  auto description = Describe(cache, key, parent, bytesMost);
  if (!description) { return std::unexpected(description.error()); }
  if (!*description) { return std::optional<Product>{}; }
  auto loaded = cache.Load(key, bytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) { return std::optional<Product>{}; }
  const auto &record = (**loaded).Record();
  auto product = ReadPayload((**loaded).Bytes(), bytesMost, (**description).ClusteredParts != 0);
  if (record.Kind != "native-geometry" || record.Parent != parent ||
      record.Bounds != (**description).Bounds.Through((**description).WorldFromLocal) || !product ||
      product->Mesh.parts() == 0 || product->Clusters.size() != (**description).ClusteredParts ||
      !Contains(product->Mesh, (**description).Bounds)) {
    return std::unexpected(AssetCacheError::Storage);
  }
  return Product{.Layout = **description,
                 .Mesh = std::move(product->Mesh),
                 .Clusters = std::move(product->Clusters)};
}

std::expected<void, AssetCacheError>
GeometryAsset::Publish(AssetCache &cache,
                       const std::string &key,
                       const std::string &parent,
                       const Description &description,
                       const Geometry &geometry,
                       size_t bytesMost,
                       std::span<const std::vector<DagCluster>> clusters) {
  if (!AssetProductKey::Parse(key) || (!parent.empty() && !AssetProductKey::Parse(parent)) ||
      !Valid(description) || geometry.parts() == 0 || !geometry.wellFormed() ||
      !Contains(geometry, description.Bounds) || description.ClusteredParts != clusters.size() ||
      (!clusters.empty() && !ValidGeometryClusters(geometry, clusters))) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  BinaryValueWriter out(bytesMost);
  if (!WriteDescription(out, description)) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  auto encoded = clusters.empty() ? EncodeGeometryProduct(geometry, bytesMost)
                                  : EncodeClusteredGeometry(geometry, clusters, bytesMost);
  if (!encoded) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  const auto bounds = description.Bounds.Through(description.WorldFromLocal);
  const AssetRecord record{.Key = key,
                           .Kind = "native-geometry",
                           .Bounds = bounds,
                           .Package = {},
                           .ByteCount = encoded->size(),
                           .Parent = parent};
  auto stored = cache.Publish(std::span(&record, 1), *encoded);
  if (!stored) { return std::unexpected(stored.error()); }
  const AssetRecord info{.Key = DescriptionKey(key),
                         .Kind = "native-geometry-info",
                         .Bounds = bounds,
                         .Package = {},
                         .ByteCount = out.Out.Bytes().size(),
                         .Parent = parent};
  return cache.Publish(std::span(&info, 1), out.Out.Bytes());
}
}
