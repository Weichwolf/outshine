#ifndef OUTSHINE_GENERATORS_WATER_WATERSURFACEPREPARATION_H
#define OUTSHINE_GENERATORS_WATER_WATERSURFACEPREPARATION_H

#include "WaterAsset.h"
#include "TangentFrame.h"
#include <expected>
#include <stop_token>
#include <string>

namespace outshine::Generators {
[[nodiscard]] std::expected<WaterAsset, std::string> PrepareWaterSurfaces(
    const WaterAsset &water, const TangentFrame &frame, const std::stop_token &stop = {});
}
#endif
