#ifndef OUTSHINE_RENDER_STAGES_DEPTHPYRAMID_H
#define OUTSHINE_RENDER_STAGES_DEPTHPYRAMID_H

#include <array>
#include <cstdint>

namespace outshine::Render {

inline constexpr uint32_t kPyramidLevels = 4u;

struct PyramidShape {
  std::array<uint32_t, kPyramidLevels> Wide = {{}};
  std::array<uint32_t, kPyramidLevels> High = {{}};
  std::array<uint32_t, kPyramidLevels> At = {{}};
  uint32_t Texels = 0;
};

[[nodiscard]] inline PyramidShape PyramidOver(uint32_t wide, uint32_t high) {
  PyramidShape out;
  uint32_t across = wide > 1u ? wide / 2u : 1u;
  uint32_t down = high > 1u ? high / 2u : 1u;
  uint32_t at = 0u;
  for (uint32_t level = 0; level < kPyramidLevels; ++level) {
    out.Wide[level] = across;
    out.High[level] = down;
    out.At[level] = at;
    at += across * down;
    across = across > 1u ? across / 2u : 1u;
    down = down > 1u ? down / 2u : 1u;
  }
  out.Texels = at;
  return out;
}

} // namespace outshine::Render
#endif
