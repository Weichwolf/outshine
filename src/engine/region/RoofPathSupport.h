#ifndef OUTSHINE_ENGINE_REGION_ROOFPATHSUPPORT_H
#define OUTSHINE_ENGINE_REGION_ROOFPATHSUPPORT_H

#include "TransportGeometry.h"
#include <atomic>
#include <span>
#include <string>

namespace outshine {
class TangentFrame;

namespace Generators {
class PreparedBuildingAssets;

namespace Osm {
struct RegionSources;
}
}

[[nodiscard]] RoadDeckProfile PrepareRoofDeckProfile(const Generators::Osm::RegionSources &sources,
                                                     Generators::PreparedBuildingAssets &assets,
                                                     std::span<const std::string> baseKeys,
                                                     const TangentFrame &frame,
                                                     const std::atomic_bool &stopping);
}

#endif
