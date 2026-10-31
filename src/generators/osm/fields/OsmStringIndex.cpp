#include "OsmStringIndex.h"
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine::Generators::Osm {
namespace {
constexpr uint32_t kEmpty = std::numeric_limits<uint32_t>::max();
constexpr size_t kInitialSlots = 8;

size_t Slots(size_t entries) noexcept {
  return entries == 0 ? 0 : std::bit_ceil(std::max(kInitialSlots, 2 * entries));
}
}

size_t OsmStringIndex::PeakBytes(size_t entries) noexcept {
  if (entries > std::numeric_limits<uint32_t>::max()) { return std::numeric_limits<size_t>::max(); }
  const auto slots = Slots(entries);
  return (slots + slots / 2) * sizeof(uint32_t);
}

size_t OsmStringIndex::CapacityBytes() const noexcept {
  return Slots_.capacity() * sizeof(uint32_t);
}

void OsmStringIndex::Clear() noexcept {
  std::vector<uint32_t>{}.swap(Slots_);
  Entries_ = 0;
}

size_t OsmStringIndex::Position(const std::vector<std::string> &pool, std::string_view text) const {
  const auto mask = Slots_.size() - 1;
  size_t at = std::hash<std::string_view>{}(text)&mask;
  while (Slots_[at] != kEmpty && pool[Slots_[at]] != text) { at = (at + 1) & mask; }
  return at;
}

void OsmStringIndex::Rebuild(const std::vector<std::string> &pool, size_t capacity) {
  Slots_.assign(capacity, kEmpty);
  for (size_t id = 0; id < pool.size(); ++id) {
    const auto at = Position(pool, pool[id]);
    if (Slots_[at] == kEmpty) { Slots_[at] = static_cast<uint32_t>(id); }
  }
  Entries_ = pool.size();
}

uint32_t OsmStringIndex::Intern(std::vector<std::string> &pool, std::string_view text) {
  if (Slots_.empty() || Entries_ != pool.size()) {
    Rebuild(pool, Slots(std::max(size_t{1}, pool.size())));
  }
  auto at = Position(pool, text);
  if (Slots_[at] != kEmpty) { return Slots_[at]; }
  assert(pool.size() < kEmpty);
  const auto required = Slots(pool.size() + 1);
  if (Slots_.size() < required) {
    Rebuild(pool, required);
    at = Position(pool, text);
  }
  const auto id = static_cast<uint32_t>(pool.size());
  std::string owned(text);
  pool.push_back(std::move(owned));
  Slots_[at] = id;
  Entries_ = pool.size();
  return id;
}
}
