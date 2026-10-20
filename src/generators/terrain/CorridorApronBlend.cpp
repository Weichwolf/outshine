#include "CorridorApronBlend.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>

namespace outshine {
namespace {
constexpr uint32_t kNoGroup = std::numeric_limits<uint32_t>::max();
}

CorridorApronBlend::CorridorApronBlend(std::span<const EarthworkStamp> stamps)
    : GroupOf_(stamps.size(), kNoGroup) {
  std::unordered_map<uint64_t, std::array<uint32_t, 2>> byKey;
  for (size_t index = 0; index < stamps.size(); ++index) {
    const auto &stamp = stamps[index];
    if (stamp.Kind != EarthworkKind::Corridor || stamp.CorridorKey == 0 || stamp.Profile) {
      continue;
    }
    auto [found, inserted] = byKey.try_emplace(stamp.CorridorKey, std::array{kNoGroup, kNoGroup});
    auto &group = found->second[stamp.Fills ? 1 : 0];
    if (group == kNoGroup) {
      group = static_cast<uint32_t>(Groups_.size());
      Groups_.emplace_back();
    }
    GroupOf_[index] = group;
  }
  Active_.reserve(Groups_.size());
}

void CorridorApronBlend::Reset() {
  for (const auto group : Active_) { Groups_[group] = {}; }
  Active_.clear();
}

bool CorridorApronBlend::Add(uint32_t stamp, Influence influence) {
  const auto group = GroupOf_[stamp];
  if (group == kNoGroup) { return false; }
  if (influence.Weight <= 0) { return true; }
  auto &into = Groups_[group];
  if (into.Weight == 0) { Active_.push_back(group); }
  into.Weight += influence.Weight;
  into.CorrectionM += influence.Weight * influence.CorrectionM;
  if (influence.Weight > into.StrongestWeight) {
    into.StrongestWeight = influence.Weight;
    into.Which = stamp;
  }
  return true;
}

CorridorApronBlend::Contribution CorridorApronBlend::Fold() const {
  Contribution result;
  for (const auto group : Active_) {
    const auto &held = Groups_[group];
    result.Weight += held.StrongestWeight;
    result.CorrectionM += held.StrongestWeight * (held.CorrectionM / held.Weight);
    if (held.StrongestWeight > result.StrongestWeight) {
      result.StrongestWeight = held.StrongestWeight;
      result.Which = held.Which;
    }
  }
  return result;
}

size_t CorridorApronBlend::HeapBytes() const noexcept {
  return (GroupOf_.capacity() + Active_.capacity()) * sizeof(uint32_t) +
         Groups_.capacity() * sizeof(Contribution);
}

}
