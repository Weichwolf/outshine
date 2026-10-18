#ifndef OUTSHINE_GENERATORS_FLORA_PLANTCROWNASSET_H
#define OUTSHINE_GENERATORS_FLORA_PLANTCROWNASSET_H

#include "PlantCrownPlan.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {

[[nodiscard]] std::expected<std::vector<uint8_t>, std::string_view>
EncodePlantCrownPlan(const PlantCrownPlan &plan, size_t bytesMost);
[[nodiscard]] std::expected<PlantCrownPlan, std::string_view>
DecodePlantCrownPlan(std::span<const uint8_t> bytes, size_t bytesMost);

}
#endif
