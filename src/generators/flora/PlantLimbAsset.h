#ifndef OUTSHINE_GENERATORS_FLORA_PLANTLIMBASSET_H
#define OUTSHINE_GENERATORS_FLORA_PLANTLIMBASSET_H

#include "PlantLimbPlan.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {
[[nodiscard]] std::expected<std::vector<uint8_t>, std::string_view>
EncodePlantLimbs(std::span<const PlantLimbPlan> limbs, size_t bytesMost);
[[nodiscard]] std::expected<std::vector<PlantLimbPlan>, std::string_view>
DecodePlantLimbs(std::span<const uint8_t> bytes, size_t residentBytesMost);
}
#endif
