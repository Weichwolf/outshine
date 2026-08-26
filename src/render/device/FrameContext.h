#ifndef OUTSHINE_RENDER_DEVICE_FRAMECONTEXT_H
#define OUTSHINE_RENDER_DEVICE_FRAMECONTEXT_H

namespace outshine::Render {

struct FrameContext {

  alignas(16) double PreViewTranslation[3];
  alignas(16) float Mvp16[16];

  alignas(16) double PrevPreViewTranslation[3];
  alignas(16) float PrevMvp16[16];
};
static_assert(alignof(FrameContext) == 16 && sizeof(FrameContext) == 192,
              "the frame's one translation and the matrix rows start on 128-bit boundaries");

}
#endif
