#ifndef OUTSHINE_WORLD_GROUND_TERRAINCERTIFICATE_H
#define OUTSHINE_WORLD_GROUND_TERRAINCERTIFICATE_H

#include "TerrainRevisionIndex.h"
#include "Address.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Ground {

class TerrainCertificate {
public:
  enum class Validation : uint8_t { Current, Unknown, Stale, ScopeChanged, Pending };

  [[nodiscard]] static TerrainCertificate
  FromDelivery(Data::TileId requested,
               std::optional<TerrainRevisionIndex::Stamp> stamp,
               uint64_t terrainScope = 0) {
    TerrainCertificate made;
    made.TerrainScope_ = terrainScope;
    if (stamp) {
      made.Complete_ = stamp->Requested == requested && stamp->Owner &&
                       stamp->RegistrationRevision != 0 && stamp->DeliveryRevision != 0;
      made.Dependencies_.push_back(std::move(*stamp));
    }
    return made;
  }

  void Merge(const TerrainCertificate &other) {
    Complete_ = Complete_ && other.Complete_ && TerrainScope_ == other.TerrainScope_;
    ScopeConsistent_ =
        ScopeConsistent_ && other.ScopeConsistent_ &&
        (TerrainScope_ == 0 || other.TerrainScope_ == 0 || TerrainScope_ == other.TerrainScope_);
    if (TerrainScope_ == 0) { TerrainScope_ = other.TerrainScope_; }
    for (const auto &stamp : other.Dependencies_) {
      if (!Dependencies_.empty() && stamp.Owner != Dependencies_.front().Owner) {
        Complete_ = false;
      }
      const auto position =
          std::ranges::lower_bound(Dependencies_, Order(stamp.Requested), {}, [](const auto &held) {
            return Order(held.Requested);
          });
      if (position != Dependencies_.end() && position->Requested == stamp.Requested) {
        if (*position != stamp) { Complete_ = false; }
      } else if (Dependencies_.size() < kDependenciesMost) {
        Dependencies_.insert(position, stamp);
      } else {
        Complete_ = false;
      }
    }
  }

  [[nodiscard]] uint64_t TerrainScopeRevision() const noexcept { return TerrainScope_; }

  [[nodiscard]] bool ScopeCurrent(uint64_t scope) const noexcept {
    return ScopeConsistent_ && (TerrainScope_ == 0 || scope == 0 || TerrainScope_ == scope);
  }

  void Invalidate() noexcept { Complete_ = false; }

  [[nodiscard]] bool IsComplete() const noexcept { return Complete_ && !Dependencies_.empty(); }

  [[nodiscard]] std::span<const TerrainRevisionIndex::Stamp> Dependencies() const noexcept {
    return Dependencies_;
  }

  [[nodiscard]] size_t Bytes() const noexcept {
    return Dependencies_.size() * sizeof(TerrainRevisionIndex::Stamp);
  }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return Dependencies_.capacity() * sizeof(TerrainRevisionIndex::Stamp);
  }

private:
  static constexpr size_t kDependenciesMost = 4096;

  [[nodiscard]] static std::tuple<int, uint32_t, uint32_t> Order(Data::TileId tile) noexcept {
    return std::tuple{tile.Zoom, tile.X, tile.Y};
  }

  std::vector<TerrainRevisionIndex::Stamp> Dependencies_;
  bool Complete_ = false;
  bool ScopeConsistent_ = true;
  uint64_t TerrainScope_ = 0;
};

}
#endif
