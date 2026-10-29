#ifndef OUTSHINE_GENERATORS_BUILDING_PREPAREDBUILDINGASSETS_H
#define OUTSHINE_GENERATORS_BUILDING_PREPAREDBUILDINGASSETS_H

#include "AssetCache.h"
#include "PreparedStructureTile.h"
#include "PreparedBuildingBasis.h"
#include "Box.h"
#include <cstddef>
#include "TilePool.h"
#include <atomic>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Generators {
class PreparedBuildingAssets {
public:
  using Base = std::shared_ptr<const PreparedStructureTile>;
  using Basis = std::shared_ptr<const PreparedBuildingBasis>;

  class BaseWrite {
  public:
    BaseWrite(BaseWrite &&) noexcept;
    BaseWrite &operator=(BaseWrite &&) noexcept;
    ~BaseWrite();
    [[nodiscard]] size_t PublicationBytes() const noexcept;

    [[nodiscard]] uint64_t PublicationAttempts() const noexcept { return Attempts_; }

  private:
    friend class PreparedBuildingAssets;
    struct State;
    explicit BaseWrite(std::unique_ptr<State> state);
    std::unique_ptr<State> State_;
    uint64_t Attempts_ = 0;
  };

  struct GeometryWrite {
    std::string Key;
    PreparedAssetWrite Package;
  };

  struct RequestedBasis {
    std::string BaseKey;
    Basis Product;
    Box Bounds;
  };

  struct Counters {
    uint64_t Hits = 0, Misses = 0, Writes = 0, ReadBytes = 0;
    uint64_t BasisHits = 0, BasisMisses = 0, BasisWrites = 0, BasisReadBytes = 0;
    uint64_t BasisResidentHits = 0;
    uint64_t GeometryHits = 0, GeometryMisses = 0, GeometryWrites = 0, GeometryReadBytes = 0;
    uint64_t SurfaceHits = 0, SurfaceMisses = 0, SurfaceWrites = 0, SurfaceReadBytes = 0;
    uint64_t GeometryPreparations = 0, BasePreparations = 0, BasePackagePreparations = 0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedBuildingAssets>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources);
  [[nodiscard]] std::string Key(Data::TileId tile,
                                uint64_t streetDigest,
                                const ::outshine::Ground::ShapedGround &shape,
                                double spanM,
                                std::string_view inputDigest = {}) const;
  // Pin demand lookup to the same source-input digest as the content key.
  [[nodiscard]] std::string RequestKey(Data::TileId tile,
                                       const ::outshine::Ground::ShapedGround &shape,
                                       double spanM,
                                       std::string_view inputDigest) const;
  [[nodiscard]] std::expected<std::optional<RequestedBasis>, StructureBakeError>
  LoadBasisRequest(const std::string &requestKey);
  [[nodiscard]] std::expected<Basis, StructureBakeError>
  LoadBasis(const std::string &key, const std::string &requestKey = {});
  [[nodiscard]] std::expected<void, StructureBakeError>
  BindBasisRequest(const std::string &key, const std::string &requestKey);
  // True means no usable basis remains; false preserves a concurrently repaired base.
  [[nodiscard]] std::expected<bool, StructureBakeError>
  RetireUnavailableBasis(const std::string &key);
  [[nodiscard]] std::expected<Base, StructureBakeError> Load(const std::string &key);
  [[nodiscard]] std::expected<Base, StructureBakeError>
  Generate(const std::string &key,
           const RawTile &raw,
           const ::outshine::Ground::HeightField &heights,
           const std::atomic_bool &stopping,
           const std::string &requestKey = {});
  [[nodiscard]] std::expected<BaseWrite, StructureBakeError>
  PrepareBase(const std::string &key,
              const RawTile &raw,
              const ::outshine::Ground::HeightField &heights,
              const std::atomic_bool &stopping,
              const std::string &requestKey = {});
  // One package attempt or final restore; an empty Base means more work remains.
  [[nodiscard]] std::expected<Base, StructureBakeError>
  ResumeBase(BaseWrite &prepared, const std::atomic_bool &stopping);
  [[nodiscard]] std::expected<std::optional<BakedTile>, StructureBakeError>
  LoadGeometry(const std::string &baseKey, const PreparedStructureTile &base, const RawTile &view);
  [[nodiscard]] std::expected<std::optional<BakedTile>, StructureBakeError>
  LoadGeometry(const std::string &baseKey, Basis basis, const RawTile &view);
  [[nodiscard]] std::expected<std::string, StructureBakeError>
  StoreGeometry(const std::string &baseKey,
                const PreparedStructureTile &base,
                const RawTile &view,
                const BakedTile &tile);
  [[nodiscard]] std::expected<GeometryWrite, StructureBakeError>
  PrepareGeometry(const std::string &baseKey,
                  const PreparedStructureTile &base,
                  const RawTile &view,
                  const BakedTile &tile);
  [[nodiscard]] std::expected<void, StructureBakeError>
  PublishGeometry(const GeometryWrite &prepared);
  [[nodiscard]] Counters Costs() const noexcept;

private:
  [[nodiscard]] static StructureBakeError CacheFailure(AssetCacheError error) noexcept;
  static constexpr size_t kPackageBytesMost = size_t{64} * 1024 * 1024;
  static constexpr size_t kResidentBytesMost = size_t{128} * 1024 * 1024;
  static constexpr size_t kResidentBasisEntriesMost = 256;
  [[nodiscard]] static std::string BasisKey(const std::string &key);
  [[nodiscard]] std::expected<void, StructureBakeError>
  StoreBasis(const std::string &key,
             const Box &bounds,
             const PreparedBuildingBasis &basis,
             const std::string &requestKey = {});
  [[nodiscard]] std::expected<Basis, StructureBakeError>
  LoadCachedBasis(const std::string &key, const std::string &requestKey);
  [[nodiscard]] std::expected<std::optional<BakedTile>, StructureBakeError>
  LoadGeometryArtifact(const std::string &baseKey, const RawTile &view, size_t coordinatePoints);

  struct SurfaceCosts {
    std::atomic_uint64_t Hits{0}, Misses{0}, Writes{0}, ReadBytes{0};
  };

  struct ResidentBasis {
    AssetRecord Record;
    std::weak_ptr<const PreparedBuildingBasis> Product;
  };

  PreparedBuildingAssets(std::unique_ptr<AssetCache> cache, std::string recipe);
  [[nodiscard]] static BaseWrite
  PrepareBaseWrite(std::string key, PreparedStructureTile base, std::string requestKey);
  [[nodiscard]] std::expected<Base, StructureBakeError>
  RestoreBase(BaseWrite &prepared, const std::atomic_bool &stopping);
  [[nodiscard]] std::expected<Base, StructureBakeError>
  CompleteBase(BaseWrite &prepared, const std::atomic_bool &stopping);
  [[nodiscard]] std::expected<PreparedAssetWrite, StructureBakeError>
  PrepareBasePackage(BaseWrite::State &state);
  [[nodiscard]] std::expected<PreparedAssetWrite, StructureBakeError>
  PrepareBasis(const std::string &key,
               const Box &bounds,
               const PreparedBuildingBasis &basis,
               const std::string &requestKey);
  [[nodiscard]] std::expected<PreparedAssetWrite, StructureBakeError>
  PrepareSurfaceCell(const std::string &key,
                     uint32_t cell,
                     const Box &bounds,
                     std::span<const BuildingSurface *const> surfaces);
  [[nodiscard]] static Box Bounds(const PreparedStructureTile &base);
  [[nodiscard]] static Box StructureBounds(const PreparedStructureTile &base, size_t index);
  [[nodiscard]] static std::string SurfaceKey(const std::string &baseKey, uint32_t cell);
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
  std::unordered_map<std::string, ResidentBasis> ResidentBases_;
  SurfaceCosts SurfaceCosts_;
  std::atomic_uint64_t BasisHits_{0}, BasisMisses_{0}, BasisWrites_{0}, BasisReadBytes_{0};
  std::atomic_uint64_t BasisResidentHits_{0};
  std::atomic_uint64_t Hits_{0}, Misses_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic_uint64_t GeometryHits_{0}, GeometryMisses_{0}, GeometryWrites_{0},
      GeometryReadBytes_{0};
  std::atomic_uint64_t GeometryPreparations_{0}, BasePreparations_{0}, BasePackagePreparations_{0};
};
}
#endif
