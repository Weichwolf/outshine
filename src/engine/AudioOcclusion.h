#ifndef OUTSHINE_ENGINE_AUDIOOCCLUSION_H
#define OUTSHINE_ENGINE_AUDIOOCCLUSION_H

#include "TriangleBvh.h"
#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace outshine {
class Geometry;

namespace Core {
[[nodiscard]] std::expected<TriangleBvh, std::string>
BuildAudioOcclusion(const Geometry &geometry,
                    std::span<const float> groundPositionsM = {},
                    std::span<const uint32_t> groundIndices = {});
}
}
#endif
