#ifndef OUTSHINE_RENDER_TERRAIN_CLIPMAPVISIBILITY_H
#define OUTSHINE_RENDER_TERRAIN_CLIPMAPVISIBILITY_H

#include "ClipmapRing.h"
#include "ClipmapTopology.h"
#include "Mat4.h"
#include <array>

namespace outshine::Render {
[[nodiscard]] std::array<float, 2> ClipmapHeightRange(const ClipmapHeightCache &heights,
                                                      uint32_t level) noexcept;

class ClipmapFrustum {
public:
  explicit ClipmapFrustum(const Mat4f &clipFromRelative) noexcept;

  // Heights include both the level and its morph parent; unknown bounds stay visible.
  [[nodiscard]] bool Intersects(const ClipmapBlock &block,
                                const ClipmapRing &ring,
                                std::array<float, 2> heights,
                                float heightShift) const noexcept;

private:
  struct Plane {
    std::array<double, 4> Equation{}, Error{};
  };

  std::array<Plane, 4> Planes_{};
  bool Valid_ = true;
};
}
#endif
