#ifndef OUTSHINE_GENERATORS_WATER_WATERBASINSTAMPS_H
#define OUTSHINE_GENERATORS_WATER_WATERBASINSTAMPS_H

#include "WaterAsset.h"
#include "TangentFrame.h"
#include "GroundMesher.h"
#include <vector>

namespace outshine::Generators {
void AppendWaterBasinStamps(const WaterAsset &water,
                            const TangentFrame &frame,
                            double depthM,
                            std::vector<EarthworkStamp> &stamps);
}
#endif
