#ifndef OUTSHINE_RENDER_STAGES_DEPTHPYRAMID_H
#define OUTSHINE_RENDER_STAGES_DEPTHPYRAMID_H

#include <array>
#include <cstdint>

#include "TexelChain.h"

namespace outshine::Render {

inline constexpr uint32_t kPyramidLevels = 4u;

struct PyramidShape {
  uint32_t SourceWide = 0;
  uint32_t SourceHigh = 0;
  std::array<uint32_t, kPyramidLevels> Wide = {{}};
  std::array<uint32_t, kPyramidLevels> High = {{}};
  std::array<uint32_t, kPyramidLevels> At = {{}};
  uint32_t Texels = 0;
};

[[nodiscard]] inline PyramidShape PyramidOver(Texels of) {
  PyramidShape out;
  out.SourceWide = of.WidthPx;
  out.SourceHigh = of.HeightPx;
  uint32_t across = of.WidthPx > 1u ? of.WidthPx / 2u + of.WidthPx % 2u : 1u;
  uint32_t down = of.HeightPx > 1u ? of.HeightPx / 2u + of.HeightPx % 2u : 1u;
  uint32_t at = 0u;
  for (uint32_t level = 0; level < kPyramidLevels; ++level) {
    out.Wide[level] = across;
    out.High[level] = down;
    out.At[level] = at;
    at += across * down;
    across = across / 2u + across % 2u;
    down = down / 2u + down % 2u;
  }
  out.Texels = at;
  return out;
}

}
#endif
