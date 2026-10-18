#ifndef OUTSHINE_TEST_NATIVE_TREE_DEPTH_NORMALS_H
#define OUTSHINE_TEST_NATIVE_TREE_DEPTH_NORMALS_H

#include "DepthSurfacePatches.h"
#include "ImpostorAtlas.h"
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

namespace outshine::NativeTreeDetailProbe {
inline std::vector<Render::DepthSurfacePatch>
NormalPatches(const Content::ImpostorAtlas &atlas,
              const Content::ImpostorAtlas::View &source,
              std::span<const Render::DepthSurfacePatch> patches) {
  const auto pixels = static_cast<uint32_t>(atlas.Pixels());
  const Vec3 toward = source.TowardEye;
  const Vec3 right{{toward[2], 0, -toward[0]}};
  constexpr double kAngleRad = 0.26;
  const double minimumDot = std::cos(kAngleRad);
  std::vector<Render::DepthSurfacePatch> result;
  for (const auto &patch : patches) {
    Vec3 normal =
        toward + right * (-2.0 * pixels * patch.SlopeX) + Vec3{{0, 2.0 * pixels * patch.SlopeY, 0}};
    bool fits = Normalise(normal);
    for (uint32_t y = patch.Top; y < patch.Bottom; ++y) {
      for (uint32_t x = patch.Left; x < patch.Right; ++x) {
        const auto &texel = source.Texels[static_cast<size_t>(y) * pixels + x];
        if (texel.Surface == 0) { continue; }
        const Vec3 captured{{texel.Normal[0], texel.Normal[1], texel.Normal[2]}};
        fits = fits && std::abs(Dot(normal, captured)) >= minimumDot;
      }
    }
    if (fits) {
      result.push_back(patch);
      continue;
    }
    for (uint32_t y = patch.Top; y < patch.Bottom; ++y) {
      for (uint32_t x = patch.Left; x < patch.Right; ++x) {
        const auto &texel = source.Texels[static_cast<size_t>(y) * pixels + x];
        if (texel.Surface == 0) { continue; }
        result.push_back(
            {.Left = x, .Top = y, .Right = x + 1, .Bottom = y + 1, .OriginDepth = texel.Depth});
      }
    }
  }
  return result;
}
}
#endif
