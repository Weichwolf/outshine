#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGCANOPY_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGCANOPY_H

#include <atomic>
#include <span>

namespace outshine {
struct WayLine;

namespace Generators {
struct PreparedStructureTile;

struct CanopyHeights {
  double MinimumM;
  double TopM;
};

[[nodiscard]] bool IsCanopySlab(CanopyHeights heights) noexcept;
[[nodiscard]] bool AttachCanopySupports(PreparedStructureTile &tile,
                                        std::span<const WayLine> ways,
                                        const std::atomic_bool *stopping);
}
}
#endif
