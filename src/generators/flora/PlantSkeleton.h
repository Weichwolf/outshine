#ifndef OUTSHINE_GENERATORS_FLORA_PLANTSKELETON_H
#define OUTSHINE_GENERATORS_FLORA_PLANTSKELETON_H

#include "PlantGrowth.h"
#include <cstdint>
#include <span>

namespace outshine::Generators {

[[nodiscard]] bool ValidPlantGrowthPoint(PlantGrowthPoint point) noexcept;
[[nodiscard]] bool ValidPlantSkeleton(std::span<const PlantGrowthPoint> positions,
                                      std::span<const int32_t> parents) noexcept;

}
#endif
