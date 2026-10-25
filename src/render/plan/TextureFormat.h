#ifndef OUTSHINE_RENDER_PLAN_TEXTUREFORMAT_H
#define OUTSHINE_RENDER_PLAN_TEXTUREFORMAT_H

#include "RenderCatalogue.h"

namespace outshine::Render {
[[nodiscard]] Native::Format TextureFormatOf(TexelFormat declared) noexcept;
}
#endif
