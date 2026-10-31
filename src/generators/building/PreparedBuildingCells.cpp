#include "PreparedBuildingAssets.h"
#include "BuildingSurfaceBlock.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructurePlan.h"
#include "BuildingScratch.h"
#include "Sha256.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
#include <vector>
#include <string>
#include <utility>

namespace outshine::Generators {
struct PreparedBuildingAssets::SurfaceRead {
  std::vector<BuildingSurface> Product;
  PreparedAssetWrite Package;
};

std::string PreparedBuildingAssets::SurfaceKey(const std::string &baseKey, uint32_t cell) {
  const auto identity = "prepared-building-cell-1/" + baseKey + "/" + std::to_string(cell);
  return Sha256Hex(reinterpret_cast<const uint8_t *>(identity.data()), identity.size());
}

std::expected<std::vector<BuildingSurface>, StructureMeshError>
PreparedBuildingAssets::PublishSurfaces(SurfaceRead &read) {
  const std::scoped_lock lock(Lock_);
  const auto stored = Cache_->Publish(read.Package);
  if (!stored) {
    return std::unexpected(stored.error() == AssetCacheError::Busy
                               ? StructureMeshError::PreparedSurfaceBusy
                               : StructureMeshError::PreparedSurfaceUnavailable);
  }
  ++SurfaceCosts_.Writes;
  return std::move(read.Product);
}

std::expected<PreparedAssetWrite, StructureBakeError>
PreparedBuildingAssets::PrepareSurfaceCell(const std::string &key,
                                           uint32_t cell,
                                           const Box &bounds,
                                           std::span<const BuildingSurface *const> surfaces) {
  ++SurfaceCosts_.Preparations;
  auto bytes = EncodeBuildingSurfaceBlock(surfaces);
  if (!bytes || bytes->size() > kPackageBytesMost) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  std::vector<AssetRecord> records{{.Key = SurfaceKey(key, cell),
                                    .Kind = "building-forms",
                                    .Bounds = bounds,
                                    .Package = {},
                                    .ByteCount = bytes->size(),
                                    .Level = 1,
                                    .Parent = key}};
  auto prepared = Cache_->PreparePublication(std::move(records), std::move(*bytes));
  if (!prepared) { return std::unexpected(CacheFailure(prepared.error())); }
  return std::move(*prepared);
}

void PreparedBuildingAssets::BindSurfaces(const std::string &key,
                                          const std::shared_ptr<PreparedStructureTile> &base) {
  std::array<std::vector<uint32_t>, kStructureCellsPerTile> cells;
  for (uint32_t index = 0; index < base->Structures.size(); ++index) {
    cells[base->Structures[index].Layout.Cell.Index - 1u].push_back(index);
  }
  std::array<std::shared_ptr<BuildingSurfaceBlock>, kStructureCellsPerTile> blocks;
  for (uint32_t cell = 0; cell < cells.size(); ++cell) {
    if (cells[cell].empty()) { continue; }
    blocks[cell] = std::make_shared<BuildingSurfaceBlock>(
        [owner = Self_.lock(),
         root = std::weak_ptr(base),
         key,
         cell,
         read = std::shared_ptr<SurfaceRead>{},
         indices = std::move(cells[cell])] mutable
            -> std::expected<std::vector<BuildingSurface>, StructureMeshError> {
          const auto held = root.lock();
          if (!owner || !held) {
            return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
          }
          return owner->LoadSurfaces(key, cell + 1u, *held, indices, read);
        });
  }
  std::array<uint32_t, kStructureCellsPerTile> offsets{};
  for (size_t index = 0; index < base->Structures.size(); ++index) {
    const auto cell = base->Structures[index].Layout.Cell.Index - 1u;
    const auto offset = offsets[cell]++;
    if (base->Surfaces[index].PreparedSelection()) {
      base->Surfaces[index].BindShapes(blocks[cell], offset);
    }
  }
}

std::expected<std::vector<BuildingSurface>, StructureMeshError>
PreparedBuildingAssets::LoadSurfaces(const std::string &key,
                                     uint32_t cell,
                                     const PreparedStructureTile &base,
                                     std::span<const uint32_t> indices,
                                     std::shared_ptr<SurfaceRead> &read) {
  if (read) { return PublishSurfaces(*read); }
  {
    const std::scoped_lock lock(Lock_);
    auto loaded = Cache_->Load(SurfaceKey(key, cell), kPackageBytesMost);
    if (!loaded) { return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable); }
    if (*loaded) {
      auto surfaces = DecodeBuildingSurfaceBlock((**loaded).Bytes(), kResidentBytesMost);
      if (surfaces && surfaces->size() == indices.size()) {
        bool compatible = true;
        for (size_t index = 0; index < indices.size(); ++index) {
          compatible &=
              CompatiblePreparedSurface(base.Surfaces[indices[index]], (*surfaces)[index]);
        }
        if (compatible) {
          ++SurfaceCosts_.Hits;
          SurfaceCosts_.ReadBytes += (**loaded).Bytes().size();
          return std::move(*surfaces);
        }
      }
    }
    ++SurfaceCosts_.Misses;
  }
  std::vector<BuildingSurface> surfaces;
  surfaces.reserve(indices.size());
  BuildingScratch scratch;
  Box bounds;
  for (const auto index : indices) {
    const auto &structure = base.Structures[index];
    const auto plan = PreparedStructurePlan(
        structure,
        base.PointsLatLon,
        base.Holes,
        std::span(base.CornerAslM).subspan(structure.CornerFirst, structure.Layout.PointCount),
        base.AnchorEcef);
    auto surface = BuildingSurface::Prepare(plan, scratch);
    if (!surface && surface.error() != StructureMeshError::UnsupportedFootprint) {
      return std::unexpected(surface.error());
    }
    surfaces.push_back(surface ? std::move(*surface) : BuildingSurface{});
    if (!CompatiblePreparedSurface(base.Surfaces[index], surfaces.back())) {
      return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
    }
    bounds.Cover(StructureBounds(base, index));
  }
  std::vector<const BuildingSurface *> references;
  references.reserve(surfaces.size());
  for (const auto &surface : surfaces) { references.push_back(&surface); }
  auto prepared = PrepareSurfaceCell(key, cell, bounds, references);
  if (!prepared) { return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable); }
  read = std::make_shared<SurfaceRead>(
      SurfaceRead{.Product = std::move(surfaces), .Package = std::move(*prepared)});
  return PublishSurfaces(*read);
}

}
