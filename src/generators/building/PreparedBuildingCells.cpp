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
namespace {}

std::string PreparedBuildingAssets::SurfaceKey(const std::string &baseKey, uint32_t cell) {
  const auto identity = "prepared-building-cell-1/" + baseKey + "/" + std::to_string(cell);
  return Sha256Hex(reinterpret_cast<const uint8_t *>(identity.data()), identity.size());
}

std::expected<void, StructureBakeError>
PreparedBuildingAssets::StoreSurfaceCell(const std::string &key,
                                         uint32_t cell,
                                         const Box &bounds,
                                         std::span<const BuildingSurface *const> surfaces) {
  auto bytes = EncodeBuildingSurfaceBlock(surfaces);
  if (!bytes || bytes->size() > kPackageBytesMost) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  const AssetRecord record{.Key = SurfaceKey(key, cell),
                           .Kind = "building-forms",
                           .Bounds = bounds,
                           .Package = {},
                           .ByteCount = bytes->size(),
                           .Level = 1,
                           .Parent = key};
  const std::scoped_lock lock(Lock_);
  if (!Cache_->Publish(std::span(&record, 1), *bytes)) {
    return std::unexpected(StructureBakeErrorKind::ArtifactFailure);
  }
  ++SurfaceCosts_.Writes;
  return {};
}

std::expected<void, StructureBakeError>
PreparedBuildingAssets::StoreSurfaces(const std::string &key, const PreparedStructureTile &base) {
  std::array<std::vector<const BuildingSurface *>, kStructureCellsPerTile> cells;
  std::array<Box, kStructureCellsPerTile> bounds;
  for (size_t index = 0; index < base.Structures.size(); ++index) {
    const auto cell = base.Structures[index].Layout.Cell.Index - 1u;
    cells[cell].push_back(&base.Surfaces[index]);
    bounds[cell].Cover(StructureBounds(base, index));
  }
  for (uint32_t cell = 0; cell < cells.size(); ++cell) {
    if (cells[cell].empty()) { continue; }
    auto stored = StoreSurfaceCell(key, cell + 1u, bounds[cell], cells[cell]);
    if (!stored) { return stored; }
  }
  return {};
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
         indices = std::move(
             cells[cell])] -> std::expected<std::vector<BuildingSurface>, StructureMeshError> {
          const auto held = root.lock();
          if (!owner || !held) {
            return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
          }
          return owner->LoadSurfaces(key, cell + 1u, *held, indices);
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
                                     std::span<const uint32_t> indices) {
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
  if (!StoreSurfaceCell(key, cell, bounds, references)) {
    return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
  }
  return surfaces;
}

}
