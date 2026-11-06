#include "PreparedBuildingAssets.h"
#include "StructureArtifact.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <algorithm>
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
#include <string_view>
#include <utility>

namespace outshine::Generators {
namespace {
std::string GeometryKey(const std::string &base, const RawTile &view) {
  ByteWriter bytes(4096);
  const auto text = [&bytes](std::string_view value) {
    return bytes.Number(value.size()) &&
           bytes.Put({reinterpret_cast<const uint8_t *>(value.data()), value.size()});
  };
  const auto number = [&bytes](double value) {
    return std::isfinite(value) && bytes.Number(value == 0.0 ? 0.0 : value);
  };
  const bool intrinsic = view.RequestedDetail.has_value();
  if (!text(intrinsic ? "building-static-lod-3" : "building-lod-6") || !text(base) ||
      !bytes.Number(view.RequestedDetail ? static_cast<int>(*view.RequestedDetail) : -1) ||
      !bytes.Number(view.RequestedCell.value_or(0)) || !bytes.Number(view.ClusterTriangles)) {
    return {};
  }
  if (!intrinsic) {
    if (!number(view.Projection.FocalPx) || !number(view.Projection.AllowedErrorPx) ||
        !number(view.Eye.LongitudeDeg) || !number(view.Eye.LatitudeDeg) ||
        !bytes.Number(static_cast<uint8_t>(view.EyeEcef.has_value()))) {
      return {};
    }
    if (view.EyeEcef) {
      for (const double value : *view.EyeEcef) {
        if (!number(value)) { return {}; }
      }
    }
  }
  return Sha256Hex(bytes.Bytes().data(), bytes.Bytes().size());
}

Box GeometryBounds(const BakedTile &tile, const Vec3 &anchor) {
  Box bounds;
  const auto cover = [&](const auto &vertices) {
    for (const auto &vertex : vertices) {
      bounds.Cover(anchor + Vec3{{vertex.pos[0], vertex.pos[1], vertex.pos[2]}});
    }
  };
  cover(tile.Built.WallCorners);
  cover(tile.Built.RoofCorners);
  if (bounds.Empty()) { bounds.Cover(anchor); }
  return bounds;
}

bool ValidCeilings(const BakedTile &tile, size_t points) {
  return std::ranges::all_of(tile.Prints, [points](const auto &print) {
    return !print.TerrainCeiling || print.TerrainCeiling->OriginPoint < points;
  });
}

}

void PreparedBuildingAssets::RestoreCoordinates(BakedTile &tile,
                                                const PreparedStructureTile &base) {
  tile.Coordinates = std::make_shared<Ground::BuildingGeometry>();
  tile.Coordinates->Origin = base.Origin;
  tile.Coordinates->OwnCoordinates(base.PointsLatLon, base.Holes);
  if (base.Origin.Provenance) {
    for (const auto &structure : base.Structures) {
      if (!tile.RequestedCell || structure.Layout.Cell.Index == *tile.RequestedCell) {
        tile.Coordinates->Sources.push_back(structure.Layout.SourceId);
      }
    }
  }
}

void PreparedBuildingAssets::RestoreCoordinates(BakedTile &tile, Basis basis) {
  tile.Coordinates = std::make_shared<Ground::BuildingGeometry>();
  tile.Coordinates->Origin = basis->Origin;
  if (basis->Origin.Provenance) {
    for (const auto &source : basis->Sources) {
      if (!tile.RequestedCell || source.Cell == *tile.RequestedCell) {
        tile.Coordinates->Sources.push_back(source.Id);
      }
    }
  }
  const std::span<const double> points = basis->PointsLatLon;
  const std::span<const GeographicRing> rings = basis->Holes;
  const size_t retainedBytes = basis->HeapBytes();
  tile.Coordinates->ShareCoordinates(std::move(basis), points, rings, retainedBytes);
}

std::expected<std::optional<BakedTile>, StructureBakeError>
PreparedBuildingAssets::LoadGeometryArtifact(const std::string &baseKey,
                                             const RawTile &view,
                                             size_t coordinatePoints) {
  const auto key = GeometryKey(baseKey, view);
  if (key.empty()) { return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct); }
  const auto loaded = Cache_->Load(key, kStructureArtifactBytesMost);
  if (!loaded) { return std::unexpected(CacheFailure(loaded.error())); }
  const std::string_view kind = view.RequestedDetail ? "building-static-lod" : "building-lod";
  if (*loaded && (**loaded).Record().Parent == baseKey && (**loaded).Record().Kind == kind &&
      (**loaded).Record().Level ==
          static_cast<uint32_t>(view.RequestedDetail.value_or(LevelOfDetail::Fine))) {
    auto tile = DecodeStructureArtifact((**loaded).Bytes(), key, 0);
    if (tile && ValidCeilings(*tile, coordinatePoints)) {
      ++GeometryHits_;
      GeometryReadBytes_ += (**loaded).Bytes().size();
      return std::move(tile);
    }
  }
  ++GeometryMisses_;
  return std::optional<BakedTile>{};
}

std::expected<std::optional<BakedTile>, StructureBakeError> PreparedBuildingAssets::LoadGeometry(
    const std::string &baseKey, const PreparedStructureTile &base, const RawTile &view) {
  auto loaded = LoadGeometryArtifact(baseKey, view, base.PointsLatLon.size() / 2u);
  if (loaded && *loaded) { RestoreCoordinates(**loaded, base); }
  return loaded;
}

std::expected<std::optional<BakedTile>, StructureBakeError>
PreparedBuildingAssets::LoadGeometry(const std::string &baseKey, Basis basis, const RawTile &view) {
  if (!basis) { return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct); }
  auto loaded = LoadGeometryArtifact(baseKey, view, basis->PointsLatLon.size() / 2u);
  if (loaded && *loaded) { RestoreCoordinates(**loaded, std::move(basis)); }
  return loaded;
}

std::expected<PreparedBuildingAssets::ProductWrite, StructureBakeError>
PreparedBuildingAssets::PrepareGeometry(const std::string &baseKey,
                                        const PreparedStructureTile &base,
                                        const RawTile &view,
                                        const BakedTile &tile) {
  auto key = GeometryKey(baseKey, view);
  if (key.empty() || !ValidCeilings(tile, base.PointsLatLon.size() / 2u)) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  ++GeometryPreparations_;
  return PrepareProduct(
      {.Key = std::move(key),
       .Kind = view.RequestedDetail ? "building-static-lod" : "building-lod",
       .Bounds = view.RequestedDetail ? GeometryBounds(tile, base.AnchorEcef) : Bounds(base),
       .Package = {},
       .Level = static_cast<uint32_t>(view.RequestedDetail.value_or(LevelOfDetail::Fine)),
       .Parent = baseKey},
      tile,
      ProductKind::Geometry);
}

std::expected<PreparedBuildingAssets::ProductWrite, StructureBakeError>
PreparedBuildingAssets::PrepareProduct(AssetRecord record,
                                       const BakedTile &tile,
                                       ProductKind kind) {
  auto encoded = EncodeStructureArtifact(tile, record.Key);
  if (!encoded) {
    if (encoded.error() == StructureArtifactError::CapacityExceeded) {
      return std::unexpected(StructureBakeErrorKind::ArtifactCapacityExceeded);
    }
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  if (encoded->size() > kStructureArtifactBytesMost) {
    return std::unexpected(StructureBakeErrorKind::ArtifactCapacityExceeded);
  }
  auto key = record.Key;
  record.ByteCount = encoded->size();
  auto prepared = Cache_->PreparePublication({std::move(record)}, std::move(*encoded));
  if (!prepared) { return std::unexpected(CacheFailure(prepared.error())); }
  if (kind == ProductKind::Geometry) {
    const auto bytes = static_cast<uint64_t>(prepared->CapacityBytes());
    auto previous = GeometryPublicationPeakBytes_.load(std::memory_order_relaxed);
    while (previous < bytes && !GeometryPublicationPeakBytes_.compare_exchange_weak(
                                   previous, bytes, std::memory_order_relaxed)) {}
  }
  return ProductWrite{.Key = std::move(key), .Package = std::move(*prepared), .Kind = kind};
}

std::expected<void, StructureBakeError>
PreparedBuildingAssets::PublishProduct(const ProductWrite &prepared) {
  if (prepared.Key.empty()) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  const std::scoped_lock lock(Lock_);
  if (prepared.Kind == ProductKind::Geometry) { ++GeometryPublicationAttempts_; }
  const auto stored = Cache_->Publish(prepared.Package);
  if (!stored) {
    if (stored.error() == AssetCacheError::Busy && prepared.Kind == ProductKind::Geometry) {
      ++GeometryPublicationDeferred_;
    }
    return std::unexpected(CacheFailure(stored.error()));
  }
  if (prepared.Kind == ProductKind::Geometry) {
    ++GeometryWrites_;
  } else {
    ++ContactCosts_.Writes;
  }
  return {};
}
}
