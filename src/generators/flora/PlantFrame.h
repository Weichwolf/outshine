#ifndef OUTSHINE_GENERATORS_FLORA_PLANTFRAME_H
#define OUTSHINE_GENERATORS_FLORA_PLANTFRAME_H

#include "math/Vec3.h"
#include <array>
#include <optional>

namespace outshine::Generators {

[[nodiscard]] Vec3 PlantRotate(const std::array<double, 9> &frame, const Vec3 &point) noexcept;
[[nodiscard]] std::optional<std::array<double, 9>> PlantNormalFrame(Vec3 direction,
                                                                    const Vec3 &hint) noexcept;
[[nodiscard]] std::array<double, 9> PlantComposeFrames(const std::array<double, 9> &outer,
                                                       const std::array<double, 9> &inner) noexcept;

}
#endif
