#ifndef OUTSHINE_GENERATORS_TERRAIN_PREPAREDTERRAINASSETS_H
#define OUTSHINE_GENERATORS_TERRAIN_PREPAREDTERRAINASSETS_H

#include "AssetCache.h"
#include "TerrainTiles.h"
#include "PreparedTerrainDeformation.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Generators {

class GroundPatch;

class PreparedTerrainAssets {
public:
  struct Counters {
    uint64_t Hits = 0, Misses = 0, Resident = 0, Writes = 0, ReadBytes = 0;
    uint64_t FieldPreparations = 0, FieldPublicationAttempts = 0;
    uint64_t DeformationHits = 0, DeformationMisses = 0, DeformationWrites = 0,
             DeformationReadBytes = 0;
    uint64_t DeformationPreparations = 0, DeformationPublicationAttempts = 0;
    uint64_t PatchHits = 0, PatchMisses = 0, PatchWrites = 0, PatchReadBytes = 0;
    uint64_t PatchPreparations = 0, PatchPublicationAttempts = 0, PatchDecodes = 0;
  };

  using Failure = std::variant<AssetCacheError, std::string>;

  class PatchWrite {
  public:
    [[nodiscard]] size_t PublicationBytes() const noexcept {
      return Package_ ? Package_->CapacityBytes() : 0;
    }

  private:
    friend class PreparedTerrainAssets;

    PatchWrite(std::string key, Data::TileId at, int side, PreparedAssetWrite package)
        : Key_(std::move(key)), At_(at), Side_(side), Package_(std::move(package)) {}

    std::string Key_;
    Data::TileId At_;
    int Side_;
    std::optional<PreparedAssetWrite> Package_;
    std::shared_ptr<const GroundPatch> Ready_;
    bool Published_ = false;
  };

  class DeformationWrite {
  public:
    DeformationWrite(DeformationWrite &&) noexcept = default;
    DeformationWrite &operator=(DeformationWrite &&) noexcept = default;
    DeformationWrite(const DeformationWrite &) = delete;
    DeformationWrite &operator=(const DeformationWrite &) = delete;

    // Owned buffer capacities; excludes this object and allocator overhead.
    [[nodiscard]] size_t HeapBytes() const noexcept;

    [[nodiscard]] const TerrainDeformationMetrics &Effects() const noexcept {
      return Product_.Effects;
    }

  private:
    friend class PreparedTerrainAssets;

    DeformationWrite(std::string key,
                     PreparedTerrainDeformation product,
                     std::vector<uint32_t> parts)
        : Key_(std::move(key)), Product_(std::move(product)), Parts_(std::move(parts)) {}

    std::string Key_;
    PreparedTerrainDeformation Product_;
    std::vector<uint32_t> Parts_;
    std::optional<PreparedAssetWrite> Package_;
    size_t Next_ = 0, First_ = 0;
    bool Complete_ = false;
  };

  [[nodiscard]] static std::string DescribeFailure(const Failure &failure);

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedTerrainAssets>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources);

  [[nodiscard]] std::shared_ptr<::outshine::Ground::TerrainTiles::FieldRequest>
  RequestField(Data::TileId at, const ::outshine::Ground::TerrainTiles::Shaped &shape);

  [[nodiscard]] std::expected<std::optional<PreparedTerrainDeformation>, Failure>
  LoadDeformation(const std::string &key, size_t heapBytesMost = kTerrainDeformationBytesMost);
  [[nodiscard]] static std::expected<DeformationWrite, Failure>
  PrepareDeformation(std::string key, PreparedTerrainDeformation product);
  // One bounded package per step; false keeps the owned remaining parts pending.
  [[nodiscard]] std::expected<bool, Failure> StoreDeformation(DeformationWrite &write,
                                                              const std::stop_token &stop = {});

  [[nodiscard]] std::string PatchKey(Data::TileId at,
                                     const ::outshine::Ground::TerrainTiles::Shaped &shape,
                                     int side,
                                     int blockZoom) const;
  [[nodiscard]] std::expected<std::shared_ptr<const GroundPatch>, Failure>
  LoadPatch(const std::string &key, Data::TileId at, int side);
  [[nodiscard]] std::expected<PatchWrite, Failure>
  PreparePatch(const std::string &key, Data::TileId at, const GroundPatch &patch);
  [[nodiscard]] std::expected<std::shared_ptr<const GroundPatch>, Failure>
  StorePatch(PatchWrite &write, const std::stop_token &stop = {});

  [[nodiscard]] Counters Costs() const noexcept;

private:
  PreparedTerrainAssets(std::unique_ptr<AssetCache> cache, std::string recipe);
  class FieldRead;

  struct Request {
    std::mutex Lock;
    std::weak_ptr<::outshine::Ground::TerrainTiles::FieldRequest> Field;
  };

  [[nodiscard]] std::shared_ptr<Request> RequestFor(const std::string &key);
  [[nodiscard]] std::shared_ptr<std::mutex> MutexForRequest(const std::string &key);
  [[nodiscard]] ::outshine::Ground::TerrainGrid Remember(const std::string &key,
                                                         ::outshine::Ground::TerrainField field);

  [[nodiscard]] std::expected<PreparedAssetWrite, Failure>
  PrepareDeformationPackage(const DeformationWrite &write);

  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  std::mutex Lock_;
  std::unordered_map<std::string, std::weak_ptr<Request>> Requests_;
  std::weak_ptr<PreparedTerrainAssets> Self_;
  std::unordered_map<std::string, std::weak_ptr<const ::outshine::Ground::TerrainField>> Resident_;
  std::atomic_uint64_t DeformationHits_{0}, DeformationMisses_{0}, DeformationWrites_{0},
      DeformationReadBytes_{0};
  std::atomic_uint64_t DeformationPreparations_{0}, DeformationPublicationAttempts_{0};
  std::atomic_uint64_t FieldPreparations_{0}, FieldPublicationAttempts_{0};
  std::atomic_uint64_t Hits_{0}, Misses_{0}, ResidentHits_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic_uint64_t PatchHits_{0}, PatchMisses_{0}, PatchWrites_{0}, PatchReadBytes_{0};
  std::atomic_uint64_t PatchPreparations_{0}, PatchPublicationAttempts_{0}, PatchDecodes_{0};
};

}
#endif
