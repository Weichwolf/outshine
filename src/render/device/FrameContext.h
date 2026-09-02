#ifndef OUTSHINE_RENDER_DEVICE_FRAMECONTEXT_H
#define OUTSHINE_RENDER_DEVICE_FRAMECONTEXT_H

#include <array>
#include "math/Mat4.h"
#include "math/Vec3.h"
#include "math/Vec3.h"

namespace outshine::Render {

constexpr size_t kFrameContextBytes = 192;

struct FrameContext {
  alignas(16) Vec3 PreViewTranslation;
  alignas(16) Mat4f Mvp{};

  alignas(16) Vec3 PrevPreViewTranslation;
  alignas(16) Mat4f PrevMvp{};
};

static_assert(alignof(FrameContext) == 16 && sizeof(FrameContext) == kFrameContextBytes,
              "the frame's one translation and the matrix rows start on 128-bit boundaries");

} // namespace outshine::Render
#endif
