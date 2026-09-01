#ifndef OUTSHINE_RENDER_DEVICE_FRAMECONTEXT_H
#define OUTSHINE_RENDER_DEVICE_FRAMECONTEXT_H

#include "math/Vec3.h"
#include "math/Vec3.h"

namespace outshine::Render {

struct FrameContext {
  alignas(16) Vec3 PreViewTranslation;
  alignas(16) float Mvp[16];

  alignas(16) Vec3 PrevPreViewTranslation;
  alignas(16) float PrevMvp[16];
};

static_assert(alignof(FrameContext) == 16 && sizeof(FrameContext) == 192,
              "the frame's one translation and the matrix rows start on 128-bit boundaries");

} // namespace outshine::Render
#endif
