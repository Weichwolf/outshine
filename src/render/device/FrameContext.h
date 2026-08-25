#ifndef OUTSHINE_RENDER_DEVICE_FRAMECONTEXT_H
#define OUTSHINE_RENDER_DEVICE_FRAMECONTEXT_H

namespace outshine::Render {

struct FrameContext {

  alignas(16) double Eye[3];
  alignas(16) float Mvp16[16];

  alignas(16) double PrevEye[3];
  alignas(16) float PrevMvp16[16];
};
static_assert(alignof(FrameContext) == 16 && sizeof(FrameContext) == 192,
              "eye and matrix rows start on 128-bit boundaries; 16 bytes over the packed 176");

}
#endif
