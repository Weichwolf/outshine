#ifndef OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGERECIPECODEC_H
#define OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGERECIPECODEC_H

#include "RoadPageRecipe.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators {
struct RoadPageAddress;

struct RoadPageRecipeCodecLimits {
  size_t BytesMost = 0;
  size_t CapacityMost = 0;
};

enum class RoadPageRecipeCodecError {
  Malformed,
  CapacityExceeded,
  UnsupportedFormat,
  WrongProduct
};

[[nodiscard]] size_t RoadPageRecipeCapacityBytes(const RoadPageRecipe &recipe);
[[nodiscard]] std::string RoadPageRecipeKey(const RoadPageAddress &address);
[[nodiscard]] std::expected<size_t, RoadPageRecipeCodecError>
RoadPageRecipeDecodeCapacity(size_t bytes);
[[nodiscard]] std::expected<std::vector<uint8_t>, RoadPageRecipeCodecError>
EncodeRoadPageRecipe(std::string_view key, const RoadPageRecipe &recipe, size_t bytesMost);
[[nodiscard]] std::expected<RoadPageRecipe, RoadPageRecipeCodecError> DecodeRoadPageRecipe(
    std::string_view key, std::span<const uint8_t> bytes, RoadPageRecipeCodecLimits limits);
}
#endif
