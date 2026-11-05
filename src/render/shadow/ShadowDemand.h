#ifndef OUTSHINE_RENDER_SHADOW_SHADOWDEMAND_H
#define OUTSHINE_RENDER_SHADOW_SHADOWDEMAND_H
#include "Box.h"
#include "Mat4.h"
#include "ShadowRegionUniform.h"
#include <array>
#include <cstddef>

namespace outshine::Render {
struct ShadowDemand {
  std::array<Mat4, kSunShadowRegions> ClipFromScene{};
  size_t Regions = 0;
  Vec3 EyeM;

  [[nodiscard]] bool MayContain(const Box &sceneBounds) const noexcept;
  [[nodiscard]] bool operator==(const ShadowDemand &) const = default;
};
}
#endif
