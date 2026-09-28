#include "TerrainRevisionIndex.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <limits>
#include <tuple>

namespace outshine::Ground {
namespace {

bool ValidTile(Data::TileId tile) noexcept {
  if (tile.Zoom < 0 || tile.Zoom > Data::TileId::MaximumZoom) { return false; }
  const auto side = uint64_t{1} << static_cast<unsigned>(tile.Zoom);
  return tile.X < side && tile.Y < side;
}

auto Order(Data::TileId tile) noexcept {
  return std::tuple{tile.Zoom, tile.X, tile.Y};
}

}

TerrainRevisionIndex::TerrainRevisionIndex(size_t entriesMost) : EntriesMost_(entriesMost) {
  Entries_.reserve(entriesMost);
}

std::expected<std::unique_ptr<TerrainRevisionIndex>, TerrainRevisionIndex::Error>
TerrainRevisionIndex::Create(size_t entriesMost) {
  if (entriesMost == 0 || entriesMost > 65536) { return std::unexpected(Error::InvalidCapacity); }
  return std::unique_ptr<TerrainRevisionIndex>(new TerrainRevisionIndex(entriesMost));
}

std::expected<TerrainRevisionIndex::Stamp, TerrainRevisionIndex::Error>
TerrainRevisionIndex::IssueDeliveryStamp(Data::TileId requested) {
  if (!ValidTile(requested)) { return std::unexpected(Error::InvalidTile); }
  const std::scoped_lock lock(Mutex_);
  return Register(requested, std::nullopt);
}

std::expected<TerrainRevisionIndex::Stamp, TerrainRevisionIndex::Error>
TerrainRevisionIndex::RestoreCachedStamp(const Stamp &stamp) {
  if (stamp.Owner != Owner_ || stamp.RegistrationRevision == 0 || stamp.DeliveryRevision == 0 ||
      !ValidTile(stamp.Requested)) {
    return std::unexpected(Error::InvalidStamp);
  }
  const std::scoped_lock lock(Mutex_);
  if (stamp.DeliveryRevision > stamp.RegistrationRevision ||
      stamp.RegistrationRevision > RegistrationClock_) {
    return std::unexpected(Error::InvalidStamp);
  }
  const auto position =
      std::ranges::lower_bound(Entries_, Order(stamp.Requested), {}, [](const Entry &entry) {
        return Order(entry.Requested);
      });
  if (position != Entries_.end() && position->Requested == stamp.Requested) {
    if (position->DeliveryRevision != stamp.DeliveryRevision) {
      return std::unexpected(Error::StaleDelivery);
    }
    return Stamp{.Requested = stamp.Requested,
                 .RegistrationRevision = position->RegistrationRevision,
                 .DeliveryRevision = position->DeliveryRevision,
                 .Owner = Owner_};
  }
  return Register(stamp.Requested, stamp.DeliveryRevision);
}

std::expected<TerrainRevisionIndex::Stamp, TerrainRevisionIndex::Error>
TerrainRevisionIndex::Register(Data::TileId requested, std::optional<uint64_t> retainedDelivery) {
  if (RegistrationClock_ == std::numeric_limits<uint64_t>::max()) {
    Entries_.clear();
    return std::unexpected(Error::RevisionExhausted);
  }
  auto position = std::ranges::lower_bound(
      Entries_, Order(requested), {}, [](const Entry &entry) { return Order(entry.Requested); });
  const auto revision = ++RegistrationClock_;
  const auto delivery = retainedDelivery.value_or(revision);
  if (position != Entries_.end() && position->Requested == requested) {
    position->RegistrationRevision = revision;
    position->DeliveryRevision = delivery;
  } else {
    if (Entries_.size() == EntriesMost_) {
      Entries_.erase(std::ranges::min_element(Entries_, {}, &Entry::RegistrationRevision));
      position = std::ranges::lower_bound(Entries_, Order(requested), {}, [](const Entry &entry) {
        return Order(entry.Requested);
      });
    }
    Entries_.insert(position,
                    Entry{.Requested = requested,
                          .RegistrationRevision = revision,
                          .DeliveryRevision = delivery});
  }
  return Stamp{.Requested = requested,
               .RegistrationRevision = revision,
               .DeliveryRevision = delivery,
               .Owner = Owner_};
}

std::optional<TerrainRevisionIndex::Stamp>
TerrainRevisionIndex::CurrentStamp(Data::TileId requested) const {
  const std::scoped_lock lock(Mutex_);
  const auto position = std::ranges::lower_bound(
      Entries_, Order(requested), {}, [](const Entry &entry) { return Order(entry.Requested); });
  if (position == Entries_.end() || position->Requested != requested) { return std::nullopt; }
  return Stamp{.Requested = requested,
               .RegistrationRevision = position->RegistrationRevision,
               .DeliveryRevision = position->DeliveryRevision,
               .Owner = Owner_};
}

TerrainRevisionIndex::Validation
TerrainRevisionIndex::InspectStamps(std::span<const Stamp> stamps) const {
  if (stamps.empty()) { return Validation::Unknown; }
  const std::scoped_lock lock(Mutex_);
  auto result = Validation::Current;
  for (const auto &stamp : stamps) {
    if (stamp.Owner != Owner_ || stamp.RegistrationRevision == 0 || stamp.DeliveryRevision == 0) {
      return Validation::Stale;
    }
    const auto position =
        std::ranges::lower_bound(Entries_, Order(stamp.Requested), {}, [](const Entry &entry) {
          return Order(entry.Requested);
        });
    const bool resident = position != Entries_.end() && position->Requested == stamp.Requested;
    if (resident && position->DeliveryRevision != stamp.DeliveryRevision) {
      return Validation::Stale;
    }
    if (!resident || position->RegistrationRevision != stamp.RegistrationRevision) {
      result = Validation::Unknown;
    }
  }
  return result;
}

bool TerrainRevisionIndex::AreCurrent(std::span<const Stamp> stamps) const {
  return InspectStamps(stamps) == Validation::Current;
}

size_t TerrainRevisionIndex::EntryCount() const {
  const std::scoped_lock lock(Mutex_);
  return Entries_.size();
}

size_t TerrainRevisionIndex::PayloadCapacityBytes() const noexcept {
  return Entries_.capacity() * sizeof(Entry) + sizeof(Domain);
}

}
