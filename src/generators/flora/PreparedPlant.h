#ifndef OUTSHINE_GENERATORS_FLORA_PREPAREDPLANT_H
#define OUTSHINE_GENERATORS_FLORA_PREPAREDPLANT_H

#include "scene/Geometry.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {
struct PreparedPlantAxis {
  uint64_t Id = 0;
  uint32_t FirstIndex = 0, IndexCount = 0;
};

struct PreparedPlantLeaves {
  uint8_t Variant = 0;
  std::vector<uint64_t> Ids, Axes;
  std::vector<Mat4> Transforms;
};

struct PreparedPlant {
  std::array<uint8_t, 32> PlanIdentity{};
  uint64_t Root = 0;
  Geometry Prototypes;
  std::vector<PreparedPlantAxis> Axes;
  std::vector<PreparedPlantLeaves> Leaves;
};

[[nodiscard]] bool PreparedPlantIsValid(const PreparedPlant &plant);
[[nodiscard]] std::expected<std::vector<uint8_t>, std::string_view>
EncodePreparedPlant(const PreparedPlant &plant, size_t bytesMost);
[[nodiscard]] std::expected<PreparedPlant, std::string_view>
DecodePreparedPlant(std::span<const uint8_t> bytes, size_t bytesMost);
}
#endif
