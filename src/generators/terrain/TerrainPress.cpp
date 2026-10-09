#include "TerrainPress.h"

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace outshine::Generators {
PressedTerrain PressTerrain(std::span<const EarthworkStamp> yields,
                            Patchwork &candidate,
                            const TangentFrame &frame,
                            TerrainPageLayout layout,
                            double mostEarthworkM) {
  TerrainPressJob job(std::vector<EarthworkStamp>(yields.begin(), yields.end()),
                      candidate,
                      frame,
                      layout,
                      mostEarthworkM);
  constexpr size_t all = std::numeric_limits<size_t>::max();
  while (!job.Advance(all, all)) {}
  return job.Take();
}
}
