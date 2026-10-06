#include "PreparedBuildingAssets.h"
#include "StructureArtifact.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace outshine::Generators {
namespace {
constexpr size_t kGeometryBytesMost = size_t{64} * 1024 * 1024;

std::string GeometryKey(const std::string &base, const RawTile &view) {
  ByteWriter bytes(4096);
  const auto text = [&bytes](const std::string &value) {
    return bytes.Number(value.size()) &&
           bytes.Put({reinterpret_cast<const uint8_t *>(value.data()), value.size()});
  };
  const auto number = [&bytes](double value) {
    return std::isfinite(value) && bytes.Number(value == 0.0 ? 0.0 : value);
  };
  if (!text("building-lod-1") || !text(base) ||
      !bytes.Number(view.RequestedDetail ? static_cast<int>(*view.RequestedDetail) : -1) ||
      !bytes.Number(view.RequestedCell.value_or(0)) || !bytes.Number(view.ClusterTriangles) ||
      !number(view.Projection.FocalPx) || !number(view.Projection.AllowedErrorPx) ||
      !number(view.Eye.LongitudeDeg) || !number(view.Eye.LatitudeDeg) ||
      !bytes.Number(static_cast<uint8_t>(view.EyeEcef.has_value()))) {
    return {};
  }
  if (view.EyeEcef) {
    for (const double value : *view.EyeEcef) {
      if (!number(value)) { return {}; }
    }
  }
  return Sha256Hex(bytes.Bytes().data(), bytes.Bytes().size());
}

void RestoreCoordinates(BakedTile &tile, const PreparedStructureTile &base) {
  tile.Coordinates = std::make_shared<Ground::BuildingGeometry>();
  tile.Coordinates->Origin = base.Origin;
  tile.Coordinates->Points = base.PointsLatLon;
  tile.Coordinates->Rings = base.Holes;
  if (base.Origin.Provenance) {
    for (const auto &structure : base.Structures) {
      if (!tile.RequestedCell || structure.Layout.Cell.Index == *tile.RequestedCell) {
        tile.Coordinates->Sources.push_back(structure.Layout.SourceId);
      }
    }
  }
}
}

std::expected<std::optional<BakedTile>, StructureBakeError> PreparedBuildingAssets::LoadGeometry(
    const std::string &baseKey, const PreparedStructureTile &base, const RawTile &view) {
  const auto key = GeometryKey(baseKey, view);
  if (key.empty()) { return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct); }
  const std::scoped_lock lock(Lock_);
  const auto loaded = Cache_->Load(key, kGeometryBytesMost);
  if (!loaded) { return std::unexpected(StructureBakeErrorKind::ArtifactFailure); }
  if (*loaded) {
    auto tile = DecodeStructureArtifact((**loaded).Bytes(), key, 0);
    if (tile) {
      RestoreCoordinates(*tile, base);
      ++GeometryHits_;
      GeometryReadBytes_ += (**loaded).Bytes().size();
      return std::move(tile);
    }
  }
  ++GeometryMisses_;
  return std::optional<BakedTile>{};
}

std::expected<void, StructureBakeError>
PreparedBuildingAssets::StoreGeometry(const std::string &baseKey,
                                      const PreparedStructureTile &base,
                                      const RawTile &view,
                                      const BakedTile &tile) {
  const auto key = GeometryKey(baseKey, view);
  if (key.empty()) { return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct); }
  const auto encoded = EncodeStructureArtifact(tile, key);
  if (!encoded || encoded->size() > kGeometryBytesMost) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  const AssetRecord record{.Key = key,
                           .Kind = "building-lod",
                           .Bounds = Bounds(base),
                           .Package = {},
                           .ByteCount = encoded->size(),
                           .Parent = baseKey};
  const std::scoped_lock lock(Lock_);
  const auto stored = Cache_->Publish(std::span(&record, 1), *encoded);
  if (!stored) { return std::unexpected(StructureBakeErrorKind::ArtifactFailure); }
  ++GeometryWrites_;
  return {};
}
}
