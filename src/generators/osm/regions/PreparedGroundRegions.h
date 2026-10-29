#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDGROUNDREGIONS_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDGROUNDREGIONS_H

#include "GroundRegionAsset.h"
#include "Sha256.h"
#include "WaterAsset.h"
#include "AssetCache.h"
#include <cstdint>
#include <atomic>
#include <expected>
#include <memory>
#include <stop_token>
#include <variant>
#include <utility>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Ground {
struct ShapedGround;
struct TileSpot;
}

namespace outshine::Generators::Osm {
class OsmField;
enum class MvtSchema : uint8_t;

class PreparedGroundRegions {
public:
  static constexpr size_t PackageBytesMost = size_t{512} * 1024u * 1024u;

  struct Loaded {
    GroundRegionAsset Region;
    WaterAsset Water;
    size_t ReadBytes = 0;
    std::string Key;
  };

  struct Counters {
    uint64_t ReadBytes = 0, Decodes = 0, Preparations = 0;
  };

  [[nodiscard]] Counters Costs() const noexcept;
  using Failure = std::variant<AssetCacheError, std::string>;
  using ReadResult = std::expected<std::optional<Loaded>, Failure>;

  class StoreWork {
  public:
    // Encoded package capacity; a decoded region remains a separate owner.
    [[nodiscard]] size_t PublicationBytes() const noexcept {
      return Package_ ? Package_->CapacityBytes() : 0;
    }

    [[nodiscard]] uint64_t PublicationAttempts() const noexcept { return Attempts_; }

  private:
    friend class PreparedGroundRegions;
    enum class Phase { Publish, Restore, Bind, Complete };

    StoreWork(std::string key, std::string request)
        : Key_(std::move(key)), Request_(std::move(request)) {}

    std::string Key_, Request_;
    std::optional<PreparedAssetWrite> Package_;
    std::optional<Loaded> Loaded_;
    Phase Current_ = Phase::Publish;
    uint64_t Attempts_ = 0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedGroundRegions>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources, std::string_view rules);
  [[nodiscard]] std::expected<std::string, std::string>
  PlanRequest(LongitudeLatitude focus,
              int zoom,
              MvtSchema schema,
              int ring,
              const ::outshine::Ground::ShapedGround &shape,
              std::span<const uint8_t> parameters) const;
  [[nodiscard]] std::string RequestKey(int zoom,
                                       MvtSchema schema,
                                       const ::outshine::Ground::ShapedGround &shape,
                                       std::span<const uint8_t> parameters,
                                       std::span<const ::outshine::Ground::TileSpot> tiles) const;
  [[nodiscard]] std::string Key(const OsmField &vectors,
                                const ::outshine::Ground::ShapedGround &shape,
                                std::span<const uint8_t> parameters) const;
  [[nodiscard]] ReadResult Load(const std::string &key);
  [[nodiscard]] ReadResult LoadRequest(const std::string &requestKey);
  [[nodiscard]] std::expected<void, Failure> BindRequest(const std::string &key,
                                                         const std::string &requestKey);
  [[nodiscard]] std::expected<StoreWork, Failure> PrepareStore(const std::string &key,
                                                               const Box &bounds,
                                                               const GroundRegionAsset &region,
                                                               const WaterAsset &water,
                                                               const std::string &requestKey = {});
  [[nodiscard]] std::expected<Loaded, Failure> Store(StoreWork &work,
                                                     const std::stop_token &stop = {});

private:
  PreparedGroundRegions(std::unique_ptr<AssetCache> cache, std::string recipe);
  [[nodiscard]] ReadResult Read(const std::string &key);
  [[nodiscard]] std::expected<Loaded, Failure> ResumeStore(StoreWork &work,
                                                           const std::stop_token &stop);
  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  std::optional<Sha256Prefix> RequestPrefix_;
  size_t RequestPrefixBytes_ = 0;
  std::atomic_uint64_t ReadBytes_{0}, Decodes_{0}, Preparations_{0};
};
}
#endif
