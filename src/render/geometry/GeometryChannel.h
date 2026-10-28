#ifndef OUTSHINE_RENDER_GEOMETRY_GEOMETRYCHANNEL_H
#define OUTSHINE_RENDER_GEOMETRY_GEOMETRYCHANNEL_H
#include "RenderGeometryView.h"
#include <cstdint>
#include <span>

namespace outshine::Render {
struct GeometryChannel {
  const RenderGeometryView *From = nullptr;
  std::span<const float> RenderGeometryPart::*Channel = nullptr;
  uint32_t Wide = 0;

  static void Write(const void *carrying, float *into, uint32_t floats);
};
}
#endif
