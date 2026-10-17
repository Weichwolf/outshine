#ifndef OUTSHINE_RENDER_STAGES_GROUNDPATCHERROR_H
#define OUTSHINE_RENDER_STAGES_GROUNDPATCHERROR_H

#include "GroundInstance.h"
#include <array>
#include <cstddef>
#include <span>

namespace outshine::Render {
[[nodiscard]] std::array<float, 4> GroundPatchHeightErrors(std::span<const float> heights,
                                                           size_t patch);
[[nodiscard]] std::array<float, 4> GroundPatchWorldErrors(const GroundInstance &tile,
                                                          size_t patch,
                                                          const std::array<float, 4> &heightErrors);
}
#endif
