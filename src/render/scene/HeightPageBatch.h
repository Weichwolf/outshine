#ifndef OUTSHINE_RENDER_SCENE_HEIGHTPAGEBATCH_H
#define OUTSHINE_RENDER_SCENE_HEIGHTPAGEBATCH_H

#include <cstddef>
#include <span>

namespace outshine::Render {
inline constexpr size_t kHeightPageBatchMost = 48;
using HeightPageView = std::span<const float>;
}
#endif
