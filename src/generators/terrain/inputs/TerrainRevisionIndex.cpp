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
#include <vector>

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

size_t TerrainRevisionIndex::WorkingSet::HeapBytes() const noexcept {
  return sizeof(WorkingSet) + Tiles.capacity() * sizeof(Data::TileId);
}

TerrainRevisionIndex::TerrainRevisionIndex(size_t entriesMost)
    : EntriesMost_(entriesMost), UnreservedEntries_(entriesMost) {
  Entries_.reserve(entriesMost);
}

std::expected<std::unique_ptr<TerrainRevisionIndex>, TerrainRevisionIndex::Error>
TerrainRevisionIndex::Create(size_t entriesMost) {
  if (entriesMost == 0 || entriesMost > MaximumEntries) {
    return std::unexpected(Error::InvalidCapacity);
  }
  return std::unique_ptr<TerrainRevisionIndex>(new TerrainRevisionIndex(entriesMost));
}

std::expected<TerrainRevisionIndex::Reservation, TerrainRevisionIndex::Error>
TerrainRevisionIndex::ReserveFor(std::span<const Data::TileId> requested) {
  if (requested.empty()) { return Reservation{}; }
  if (requested.size() > MaximumEntries) { return std::unexpected(Error::InvalidCapacity); }
  if (!std::ranges::all_of(requested, ValidTile)) { return std::unexpected(Error::InvalidTile); }
  auto reservation = std::make_shared<WorkingSet>();
  reservation->Tiles.assign(requested.begin(), requested.end());
  std::ranges::sort(reservation->Tiles, {}, Order);
  reservation->Tiles.erase(std::ranges::unique(reservation->Tiles).begin(),
                           reservation->Tiles.end());
  const std::scoped_lock lock(Mutex_);
  std::vector<Data::TileId> needed = reservation->Tiles;
  for (const auto &weak : Owner_->Reservations) {
    if (auto active = weak.lock()) {
      if (std::ranges::equal(active->Tiles, reservation->Tiles)) { return active; }
      needed.insert(needed.end(), active->Tiles.begin(), active->Tiles.end());
    }
  }
  std::ranges::sort(needed, {}, Order);
  needed.erase(std::ranges::unique(needed).begin(), needed.end());
  if (needed.size() > MaximumEntries - UnreservedEntries_) {
    return std::unexpected(Error::InvalidCapacity);
  }
  const size_t capacity = needed.size() + UnreservedEntries_;
  if (capacity > EntriesMost_) {
    Entries_.reserve(capacity);
    EntriesMost_ = capacity;
  }
  std::erase_if(Owner_->Reservations, [](const auto &weak) { return weak.expired(); });
  Owner_->Reservations.push_back(reservation);
  return reservation;
}

bool TerrainRevisionIndex::IsReserved(Data::TileId requested) const {
  return std::ranges::any_of(Owner_->Reservations, [requested](const auto &weak) {
    const auto active = weak.lock();
    return active && std::ranges::binary_search(active->Tiles, Order(requested), {}, Order);
  });
}

bool TerrainRevisionIndex::EvictUnreserved() {
  auto oldest = Entries_.end();
  for (auto at = Entries_.begin(); at != Entries_.end(); ++at) {
    if (!IsReserved(at->Requested) &&
        (oldest == Entries_.end() || at->RegistrationRevision < oldest->RegistrationRevision)) {
      oldest = at;
    }
  }
  if (oldest == Entries_.end()) { return false; }
  Entries_.erase(oldest);
  return true;
}

std::expected<TerrainRevisionIndex::Stamp, TerrainRevisionIndex::Error>
TerrainRevisionIndex::IssueDeliveryStamp(Data::TileId requested,
                                         std::optional<DeliveryFingerprint> fingerprint) {
  if (!ValidTile(requested)) { return std::unexpected(Error::InvalidTile); }
  const std::scoped_lock lock(Mutex_);
  return Register(requested, std::nullopt, fingerprint);
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
TerrainRevisionIndex::Register(Data::TileId requested,
                               std::optional<uint64_t> retainedDelivery,
                               std::optional<DeliveryFingerprint> fingerprint) {
  if (RegistrationClock_ == std::numeric_limits<uint64_t>::max()) {
    Entries_.clear();
    return std::unexpected(Error::RevisionExhausted);
  }
  auto position = std::ranges::lower_bound(
      Entries_, Order(requested), {}, [](const Entry &entry) { return Order(entry.Requested); });
  if (fingerprint && position != Entries_.end() && position->Requested == requested &&
      position->Fingerprint == fingerprint) {
    return Stamp{.Requested = requested,
                 .RegistrationRevision = position->RegistrationRevision,
                 .DeliveryRevision = position->DeliveryRevision,
                 .Owner = Owner_};
  }
  const auto revision = ++RegistrationClock_;
  const auto delivery = retainedDelivery.value_or(revision);
  if (position != Entries_.end() && position->Requested == requested) {
    position->RegistrationRevision = revision;
    position->DeliveryRevision = delivery;
    position->Fingerprint = fingerprint;
  } else {
    if (Entries_.size() == EntriesMost_) {
      if (!EvictUnreserved()) { return std::unexpected(Error::InvalidCapacity); }
      position = std::ranges::lower_bound(Entries_, Order(requested), {}, [](const Entry &entry) {
        return Order(entry.Requested);
      });
    }
    Entries_.insert(position,
                    Entry{.Requested = requested,
                          .RegistrationRevision = revision,
                          .DeliveryRevision = delivery,
                          .Fingerprint = fingerprint});
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
  return InspectStampsUnderLock(stamps);
}

std::optional<TerrainRevisionIndex::Validation>
TerrainRevisionIndex::TryInspectStamps(std::span<const Stamp> stamps) const {
  if (stamps.empty()) { return Validation::Unknown; }
  const std::unique_lock lock(Mutex_, std::try_to_lock);
  if (!lock.owns_lock()) { return std::nullopt; }
  return InspectStampsUnderLock(stamps);
}

TerrainRevisionIndex::Validation
TerrainRevisionIndex::InspectStampsUnderLock(std::span<const Stamp> stamps) const {
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
  return Entries_.capacity() * sizeof(Entry) + sizeof(Domain) +
         Owner_->Reservations.capacity() * sizeof(Reservation);
}

}
