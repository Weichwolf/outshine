#ifndef OUTSHINE_CONTENT_SHADE_HALFTEXELS_H
#define OUTSHINE_CONTENT_SHADE_HALFTEXELS_H

#include "TexelChain.h"
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Core {

enum class HalfTexelError { InvalidExtent, InvalidBytes, ConversionFailed };

[[nodiscard]] std::expected<std::vector<uint8_t>, HalfTexelError>
PackHalfTexels(Texels extent, std::span<const float> pixels);

[[nodiscard]] std::expected<std::vector<float>, HalfTexelError>
UnpackHalfTexels(Texels extent, std::span<const uint8_t> pixels);

}

#endif
