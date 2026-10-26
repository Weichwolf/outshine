#ifndef OUTSHINE_RENDER_VIEW_FRAMECONTEXT_H
#define OUTSHINE_RENDER_VIEW_FRAMECONTEXT_H

#include "Mat4.h"
#include "Vec3.h"
#include "Vec4.h"

namespace outshine::Render {

struct FrameContext {
  alignas(16) Vec3 PreViewTranslation;
  alignas(16) Mat4f Mvp{};

  alignas(16) Vec3 PrevPreViewTranslation;
  alignas(16) Mat4f PrevMvp{};
  alignas(16) Vec4f ViewPosition = {{0, 0, 0, 1}};
};

}
#endif
