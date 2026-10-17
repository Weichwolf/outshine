#include "local_light_world_build.h"
#include "local_light_build.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <span>

namespace LightWorld {
namespace {
constexpr double kPositionWidth = 128;
constexpr uint64_t kCellWork = 1'000'000;
constexpr size_t kHeaderWords = 5, kSlotWords = 5;
constexpr int32_t kPositionCellLimit = 1 << 30;
constexpr std::array<uint32_t, 3> kHashMixers{0x9e3779b1u, 0x85ebca77u, 0xc2b2ae3du};

bool Valid(const Light &light) {
  return std::ranges::all_of(light, [](float x) { return std::isfinite(x); }) &&
         (light[3] == 0 || light[3] == 1 || light[3] == 2) && light[7] >= 0;
}

bool Global(const Light &light) {
  return light[3] == 0 || light[7] == 0;
}

uint32_t Key(int32_t coordinate) {
  return std::bit_cast<uint32_t>(coordinate) ^ 0x80000000u;
}

uint32_t Hash(const Cell &cell) {
  return (Key(cell[0]) * kHashMixers[0]) ^ (Key(cell[1]) * kHashMixers[1]) ^
         (Key(cell[2]) * kHashMixers[2]);
}
}

std::expected<Cell, Error> Builder::Position(const Light &light) const {
  Cell result{};
  for (size_t axis = 0; axis < 3; ++axis) {
    const double value = std::floor((light[4 + axis] + Translation[axis]) / kPositionWidth);
    if (!std::isfinite(value) || value < -kPositionCellLimit || value >= kPositionCellLimit) {
      return std::unexpected(Error::Domain);
    }
    result[axis] = static_cast<int32_t>(value);
  }
  return result;
}

void Builder::Encode(uint32_t id) {
  Local[id] = Source[id];
  Coordinates[id] = *Position(Source[id]);
  for (size_t axis = 0; axis < 3; ++axis) {
    Local[id][4 + axis] = static_cast<float>(Source[id][4 + axis] + Translation[axis] -
                                             Coordinates[id][axis] * kPositionWidth);
  }
}

bool Builder::Footprint(const Light &light, std::pmr::vector<Cell> &result) {
  result.clear();
  if (Global(light)) { return true; }
  const double radius = 1.0 / light[7];
  Vector centre{};
  std::array<int64_t, 3> first{};
  std::array<int64_t, 3> last{};
  uint64_t count = 1;
  for (size_t axis = 0; axis < 3; ++axis) {
    centre[axis] = light[4 + axis] + Translation[axis];
    const double low = std::floor(
        std::nextafter(centre[axis] - radius, -std::numeric_limits<double>::infinity()) / Width);
    const double high = std::floor(
        std::nextafter(centre[axis] + radius, std::numeric_limits<double>::infinity()) / Width);
    if (low < std::numeric_limits<int32_t>::min() || high > std::numeric_limits<int32_t>::max() ||
        high - low + 1 > kCellWork) {
      return false;
    }
    first[axis] = static_cast<int64_t>(low);
    last[axis] = static_cast<int64_t>(high);
    const auto extent = static_cast<uint64_t>(last[axis] - first[axis] + 1);
    if (count > kCellWork / extent) { return false; }
    count *= extent;
  }
  for (int64_t x = first[0]; x <= last[0]; ++x) {
    for (int64_t y = first[1]; y <= last[1]; ++y) {
      for (int64_t z = first[2]; z <= last[2]; ++z) {
        const Cell cell{static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z)};
        double distance = 0;
        for (size_t axis = 0; axis < 3; ++axis) {
          const double low = static_cast<double>(cell[axis]) * Width;
          const double high = (static_cast<double>(cell[axis]) + 1) * Width;
          const double delta = std::max({low - centre[axis], centre[axis] - high, 0.0});
          distance += delta * delta;
        }
        LightBuild::CountWork(Tests);
        if (distance <= radius * radius) { result.push_back(cell); }
      }
    }
  }
  return true;
}

void Builder::Fallback() {
  Overflow = true;
  Cells.clear();
  Assignments = 0;
  Globals.resize(Source.size());
  std::ranges::iota(Globals, uint32_t{0});
}

void Builder::Insert(const Cell &cell, uint32_t id) {
  auto &ids = Cells.try_emplace(cell).first->second;
  ids.insert(std::ranges::lower_bound(ids, id), id);
  ++Assignments;
  LightBuild::CountWork(MembershipChanges);
}

void Builder::Remove(const Cell &cell, uint32_t id) {
  const auto found = Cells.find(cell);
  auto &ids = found->second;
  ids.erase(std::ranges::lower_bound(ids, id));
  --Assignments;
  LightBuild::CountWork(MembershipChanges);
  if (ids.empty()) { Cells.erase(found); }
}

std::expected<void, Error>
Builder::Build(std::span<const Light> lights, uint32_t width, size_t budget, Vector translation) {
  if ((width != 16 && width != 32 && width != 64 && width != 128) || budget == 0 ||
      lights.size() > std::numeric_limits<uint32_t>::max() || !std::ranges::all_of(lights, Valid) ||
      !std::ranges::all_of(translation, [](double x) { return std::isfinite(x); })) {
    return std::unexpected(Error::Input);
  }
  const Vector previous = Translation;
  Translation = translation;
  for (const auto &light : lights) {
    if (!Position(light)) {
      Translation = previous;
      return std::unexpected(Error::Domain);
    }
  }
  Source.assign(lights.begin(), lights.end());
  Local.resize(lights.size());
  Coordinates.resize(lights.size());
  Cells.clear();
  Globals.clear();
  Width = width;
  IndexBytes = budget;
  Overflow = false;
  Tests = MembershipChanges = Assignments = 0;
  for (uint32_t id = 0; id < Source.size(); ++id) { Encode(id); }
  for (uint32_t id = 0; id < Source.size(); ++id) {
    if (Global(Source[id])) {
      Globals.push_back(id);
      continue;
    }
    if (!Footprint(Source[id], After) || After.size() > budget / sizeof(uint32_t) - Assignments) {
      Fallback();
      break;
    }
    for (const auto &cell : After) { Insert(cell, id); }
  }
  return {};
}

std::expected<void, Error> Builder::Update(uint32_t id, const Light &light) {
  if (id >= Source.size() || !Valid(light)) { return std::unexpected(Error::Input); }
  if (!Position(light)) { return std::unexpected(Error::Domain); }
  Tests = MembershipChanges = 0;
  const Light old = Source[id];
  const bool geometry = Global(old) != Global(light) ||
                        (!Global(light) && (light[7] != old[7] || light[4] != old[4] ||
                                            light[5] != old[5] || light[6] != old[6]));
  Source[id] = light;
  Encode(id);
  if (Overflow || !geometry) { return {}; }
  if (!Footprint(old, Before) || !Footprint(light, After)) {
    Fallback();
    return {};
  }
  size_t before = 0;
  size_t after = 0;
  while (before < Before.size() || after < After.size()) {
    if (after == After.size() || (before < Before.size() && Before[before] < After[after])) {
      Remove(Before[before++], id);
    } else if (before == Before.size() || After[after] < Before[before]) {
      Insert(After[after++], id);
    } else {
      ++before;
      ++after;
    }
  }
  if (Global(old)) { Globals.erase(std::ranges::lower_bound(Globals, id)); }
  if (Global(light)) { Globals.insert(std::ranges::lower_bound(Globals, id), id); }
  if (Assignments > IndexBytes / sizeof(uint32_t)) { Fallback(); }
  return {};
}

std::expected<void, Error> Builder::Pack(Lookup method) {
  const size_t capacity =
      method == Lookup::Hash ? std::bit_ceil(std::max(size_t{2}, 2 * Cells.size())) : Cells.size();
  const size_t slots = std::max(size_t{1}, capacity);
  const size_t globalAt = kHeaderWords + kSlotWords * slots + Assignments;
  const size_t coordinateAt = globalAt + Globals.size();
  const size_t poseAt = coordinateAt + 3 * Coordinates.size();
  const size_t total = poseAt + 3 * Coordinates.size();
  if (total > std::numeric_limits<uint32_t>::max() / 2) { return std::unexpected(Error::Size); }
  Words.assign(total, 0);
  Words[0] = static_cast<uint32_t>(globalAt);
  Words[1] = static_cast<uint32_t>(Globals.size());
  Words[2] = kHeaderWords;
  Words[3] = static_cast<uint32_t>(method == Lookup::Hash ? capacity - 1 : Cells.size());
  Words[4] = static_cast<uint32_t>(coordinateAt);
  size_t ordinal = 0;
  size_t next = kHeaderWords + kSlotWords * slots;
  for (const auto &[cell, ids] : Cells) {
    size_t slot = method == Lookup::Hash ? Hash(cell) & (capacity - 1) : ordinal;
    while (Words[kHeaderWords + kSlotWords * slot + 4] != 0) { slot = (slot + 1) & (capacity - 1); }
    const size_t at = kHeaderWords + kSlotWords * slot;
    for (size_t axis = 0; axis < 3; ++axis) { Words[at + axis] = Key(cell[axis]); }
    Words[at + 3] = static_cast<uint32_t>(next);
    Words[at + 4] = static_cast<uint32_t>(ids.size());
    std::ranges::copy(ids, Words.begin() + static_cast<std::ptrdiff_t>(next));
    next += ids.size();
    ++ordinal;
  }
  std::ranges::copy(Globals, Words.begin() + static_cast<std::ptrdiff_t>(globalAt));
  for (size_t id = 0; id < Coordinates.size(); ++id) {
    for (size_t axis = 0; axis < 3; ++axis) {
      Words[coordinateAt + 3 * id + axis] = std::bit_cast<uint32_t>(Coordinates[id][axis]);
    }
  }
  return {};
}
}
