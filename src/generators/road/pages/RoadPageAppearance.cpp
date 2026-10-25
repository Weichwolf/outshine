#include "RoadPageAppearance.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <variant>

namespace outshine::Generators {
namespace {
bool Before(const RoadPagePartAppearance &left, const RoadPagePartAppearance &right) {
  return left.Part.Kind != right.Part.Kind ? left.Part.Kind < right.Part.Kind
                                           : left.Part.Index < right.Part.Index;
}
}

bool ValidRoadPageAppearance(const RoadPageAppearance &appearance) {
  if (std::holds_alternative<LegacyRoadPageV1>(appearance)) { return true; }
  const auto *version2 = std::get_if<RoadPageAppearanceV2>(&appearance);
  return version2 != nullptr && version2->Key.size() == 64 &&
         std::ranges::any_of(version2->Key, [](char value) { return value != '0'; }) &&
         std::ranges::all_of(version2->Key, [](char value) {
           return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
         });
}

size_t RoadPageAppearanceCapacityBytes(const RoadPageAppearance &appearance) noexcept {
  const auto *version2 = std::get_if<RoadPageAppearanceV2>(&appearance);
  return version2 != nullptr ? version2->Key.capacity() : 0;
}

std::string RoadPageAppearanceKey(std::span<const RoadPagePartAppearance> parts) {
  constexpr size_t prefixBytes = 64;
  constexpr size_t partBytes = 2 * sizeof(uint32_t) + 3 * sizeof(float);
  if (parts.size() > std::numeric_limits<uint32_t>::max() ||
      parts.size() > (std::numeric_limits<size_t>::max() - prefixBytes) / partBytes ||
      std::ranges::adjacent_find(
          parts, [](const auto &a, const auto &b) { return !Before(a, b); }) != parts.end()) {
    return {};
  }
  BinaryValueWriter out(prefixBytes + parts.size() * partBytes);
  if (!out.Text("outshine-road-page-appearance") ||
      !out(uint32_t{2}, static_cast<uint32_t>(parts.size()))) {
    return {};
  }
  for (const auto &part : parts) {
    if ((part.Part.Kind != RoadPagePartKind::Interval &&
         part.Part.Kind != RoadPagePartKind::Junction) ||
        std::ranges::any_of(part.WearsLinear.Axis,
                            [](float value) { return !std::isfinite(value) || value < 0; }) ||
        !out(part.Part.Kind, part.Part.Index, part.WearsLinear.Axis)) {
      return {};
    }
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

bool WriteRoadPageAppearance(BinaryValueWriter &out, const RoadPageAppearance &appearance) {
  if (!ValidRoadPageAppearance(appearance)) { return false; }
  const auto *version2 = std::get_if<RoadPageAppearanceV2>(&appearance);
  return version2 == nullptr || out.Text(version2->Key);
}

bool ReadRoadPageAppearance(BinaryValueReader &in, bool version2, RoadPageAppearance &appearance) {
  if (!version2) {
    appearance = LegacyRoadPageV1{};
    return true;
  }
  RoadPageAppearanceV2 binding;
  if (!in.Text(binding.Key)) { return false; }
  appearance = std::move(binding);
  return ValidRoadPageAppearance(appearance);
}
}
