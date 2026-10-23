#ifndef OUTSHINE_RENDER_SHADOW_SHADOWRASTER_H
#define OUTSHINE_RENDER_SHADOW_SHADOWRASTER_H

#include "math/Mat4.h"
#include "math/Box.h"
#include "math/Vec3.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace outshine::Render {

struct ShadowRaster {
  Mat4 Projection{};
  Mat4f Packed{};
  Vec3 Anchor{};
  std::array<int64_t, 2> Offset{};
  uint32_t Side = 0;
};

struct ShadowRasterPatch {
  uint32_t X = 0;
  uint32_t Y = 0;
  uint32_t Width = 0;
  uint32_t Height = 0;
  std::array<int64_t, 2> Period{};
};

struct ShadowRasterUpdate {
  ShadowRaster Next;
  std::array<ShadowRasterPatch, 16> Patches{};
  std::array<uint32_t, 2> Origin{};
  uint64_t Texels = 0;
  size_t Count = 0;
};

[[nodiscard]] ShadowRasterUpdate PrepareShadowRaster(const ShadowRaster &accepted,
                                                     const Mat4 &projection,
                                                     const Vec3 &anchor,
                                                     uint32_t side,
                                                     bool full) noexcept;

[[nodiscard]] Mat4f ShadowPatchProjection(const ShadowRaster &raster,
                                          const ShadowRasterPatch &patch) noexcept;

[[nodiscard]] bool IncludeShadowChange(ShadowRasterUpdate &update, const Boxf &bounds) noexcept;

[[nodiscard]] Mat4f ShadowPatchCullProjection(const Mat4f &projection,
                                              const ShadowRasterPatch &patch,
                                              uint32_t side) noexcept;

}
#endif
