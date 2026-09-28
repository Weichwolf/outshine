#ifndef OUTSHINE_WORLD_GROUND_TERRAINREVISIONINDEX_H
#define OUTSHINE_WORLD_GROUND_TERRAINREVISIONINDEX_H

#include "Address.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Ground {

class TerrainRevisionIndex {
  struct Domain {};

public:
  enum class Error : uint8_t {
    InvalidCapacity,
    InvalidTile,
    InvalidStamp,
    StaleDelivery,
    RevisionExhausted
  };
  enum class Validation : uint8_t { Current, Unknown, Stale };

  struct Stamp {
    Data::TileId Requested;
    uint64_t RegistrationRevision = 0;
    uint64_t DeliveryRevision = 0;
    std::shared_ptr<const Domain> Owner;
    [[nodiscard]] bool operator==(const Stamp &) const noexcept = default;
  };

  [[nodiscard]] static std::expected<std::unique_ptr<TerrainRevisionIndex>, Error>
  Create(size_t entriesMost = 4096);
  [[nodiscard]] std::expected<Stamp, Error> IssueDeliveryStamp(Data::TileId requested);
  [[nodiscard]] std::expected<Stamp, Error> RestoreCachedStamp(const Stamp &stamp);
  [[nodiscard]] Validation InspectStamps(std::span<const Stamp> stamps) const;
  [[nodiscard]] std::optional<Stamp> CurrentStamp(Data::TileId requested) const;
  [[nodiscard]] bool AreCurrent(std::span<const Stamp> stamps) const;
  [[nodiscard]] size_t EntryCount() const;
  [[nodiscard]] size_t PayloadCapacityBytes() const noexcept;

private:
  struct Entry {
    Data::TileId Requested;
    uint64_t RegistrationRevision = 0;
    uint64_t DeliveryRevision = 0;
  };

  explicit TerrainRevisionIndex(size_t entriesMost);
  [[nodiscard]] std::expected<Stamp, Error> Register(Data::TileId requested,
                                                     std::optional<uint64_t> retainedDelivery);
  std::shared_ptr<const Domain> Owner_ = std::make_shared<const Domain>();
  mutable std::mutex Mutex_;
  std::vector<Entry> Entries_;
  size_t EntriesMost_;
  uint64_t RegistrationClock_ = 0;
};

}
#endif
