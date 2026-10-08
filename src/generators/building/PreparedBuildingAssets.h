#ifndef OUTSHINE_GENERATORS_BUILDING_PREPAREDBUILDINGASSETS_H
#define OUTSHINE_GENERATORS_BUILDING_PREPAREDBUILDINGASSETS_H

#include "content/AssetCache.h"
#include "PreparedStructureTile.h"
#include "PreparedBuildingBasis.h"
#include <cstddef>
#include "TilePool.h"
#include <atomic>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Generators {
class PreparedBuildingAssets {
public:
  using Base = std::shared_ptr<const PreparedStructureTile>;
  using Basis = std::shared_ptr<const PreparedBuildingBasis>;

  struct RequestedBasis {
    std::string BaseKey;
    Basis Product;
  };

  struct Counters {
    uint64_t Hits = 0, Misses = 0, Writes = 0, ReadBytes = 0;
    uint64_t BasisHits = 0, BasisMisses = 0, BasisWrites = 0, BasisReadBytes = 0;
    uint64_t GeometryHits = 0, GeometryMisses = 0, GeometryWrites = 0, GeometryReadBytes = 0;
    uint64_t SurfaceHits = 0, SurfaceMisses = 0, SurfaceWrites = 0, SurfaceReadBytes = 0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedBuildingAssets>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources);
  [[nodiscard]] std::string Key(Data::TileId tile,
                                uint64_t streetDigest,
                                const ::outshine::Ground::ShapedGround &shape,
                                double spanM,
                                std::string_view inputDigest = {}) const;
  [[nodiscard]] std::string
  RequestKey(Data::TileId tile, const ::outshine::Ground::ShapedGround &shape, double spanM) const;
  [[nodiscard]] std::expected<std::optional<RequestedBasis>, StructureBakeError>
  LoadBasisRequest(const std::string &requestKey);
  [[nodiscard]] std::expected<Basis, StructureBakeError>
  LoadBasis(const std::string &key, const std::string &requestKey = {});
  [[nodiscard]] std::expected<void, StructureBakeError> InvalidateBasis(const std::string &key);
  [[nodiscard]] std::expected<Base, StructureBakeError> Load(const std::string &key);
  [[nodiscard]] std::expected<Base, StructureBakeError>
  Generate(const std::string &key,
           const RawTile &raw,
           const ::outshine::Ground::HeightField &heights,
           const std::atomic_bool &stopping,
           const std::string &requestKey = {});
  [[nodiscard]] std::expected<std::optional<BakedTile>, StructureBakeError>
  LoadGeometry(const std::string &baseKey, const PreparedStructureTile &base, const RawTile &view);
  [[nodiscard]] std::expected<std::optional<BakedTile>, StructureBakeError>
  LoadGeometry(const std::string &baseKey, const PreparedBuildingBasis &basis, const RawTile &view);
  [[nodiscard]] std::expected<void, StructureBakeError>
  StoreGeometry(const std::string &baseKey,
                const PreparedStructureTile &base,
                const RawTile &view,
                const BakedTile &tile);
  [[nodiscard]] Counters Costs() const noexcept;

private:
  static constexpr size_t kPackageBytesMost = size_t{64} * 1024 * 1024;
  static constexpr size_t kResidentBytesMost = size_t{128} * 1024 * 1024;
  [[nodiscard]] static std::string BasisKey(const std::string &key);
  [[nodiscard]] std::expected<void, StructureBakeError>
  StoreBasis(const std::string &key,
             const Box &bounds,
             const PreparedBuildingBasis &basis,
             const std::string &requestKey = {});
  [[nodiscard]] std::expected<std::optional<BakedTile>, StructureBakeError>
  LoadGeometryArtifact(const std::string &baseKey, const RawTile &view);

  struct SurfaceCosts {
    std::atomic_uint64_t Hits{0}, Misses{0}, Writes{0}, ReadBytes{0};
  };

  PreparedBuildingAssets(std::unique_ptr<AssetCache> cache, std::string recipe);
  [[nodiscard]] std::expected<std::vector<uint8_t>, StructureBakeError>
  StoreBase(const std::string &key,
            const PreparedStructureTile &base,
            const std::string &requestKey = {});
  [[nodiscard]] static Box Bounds(const PreparedStructureTile &base);
  [[nodiscard]] static Box StructureBounds(const PreparedStructureTile &base, size_t index);
  [[nodiscard]] static std::string SurfaceKey(const std::string &baseKey, uint32_t cell);
  [[nodiscard]] std::expected<void, StructureBakeError>
  StoreSurfaces(const std::string &key, const PreparedStructureTile &base);
  void BindSurfaces(const std::string &key, const std::shared_ptr<PreparedStructureTile> &base);
  [[nodiscard]] std::expected<std::vector<BuildingSurface>, StructureMeshError>
  LoadSurfaces(const std::string &key,
               uint32_t cell,
               const PreparedStructureTile &base,
               std::span<const uint32_t> indices);
  [[nodiscard]] std::expected<void, StructureBakeError>
  StoreSurfaceCell(const std::string &key,
                   uint32_t cell,
                   const Box &bounds,
                   std::span<const BuildingSurface *const> surfaces);
  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  std::mutex Lock_;
  std::weak_ptr<PreparedBuildingAssets> Self_;
  SurfaceCosts SurfaceCosts_;
  std::atomic_uint64_t BasisHits_{0}, BasisMisses_{0}, BasisWrites_{0}, BasisReadBytes_{0};
  std::atomic_uint64_t Hits_{0}, Misses_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic_uint64_t GeometryHits_{0}, GeometryMisses_{0}, GeometryWrites_{0},
      GeometryReadBytes_{0};
};
}
#endif
