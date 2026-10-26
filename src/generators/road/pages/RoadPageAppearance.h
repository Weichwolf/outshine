#ifndef OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGEAPPEARANCE_H
#define OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGEAPPEARANCE_H

#include "RoadPageGeometry.h"
#include <cstddef>
#include <span>
#include <string>
#include <variant>

namespace outshine {
class BinaryValueWriter;
class BinaryValueReader;
}

namespace outshine::Generators {
struct LegacyRoadPageV1 {
  [[nodiscard]] bool operator==(const LegacyRoadPageV1 &) const = default;
};

struct RoadPageAppearanceV2 {
  std::string Key;
  [[nodiscard]] bool operator==(const RoadPageAppearanceV2 &) const = default;
};

using RoadPageAppearance = std::variant<LegacyRoadPageV1, RoadPageAppearanceV2>;

[[nodiscard]] bool ValidRoadPageAppearance(const RoadPageAppearance &appearance);
[[nodiscard]] size_t RoadPageAppearanceCapacityBytes(const RoadPageAppearance &appearance) noexcept;
[[nodiscard]] std::string RoadPageAppearanceKey(std::span<const RoadPagePartAppearance> parts);
[[nodiscard]] bool WriteRoadPageAppearance(BinaryValueWriter &out,
                                           const RoadPageAppearance &appearance);
[[nodiscard]] bool
ReadRoadPageAppearance(BinaryValueReader &in, bool version2, RoadPageAppearance &appearance);
}
#endif
