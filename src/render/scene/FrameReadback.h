#ifndef OUTSHINE_RENDER_SCENE_FRAMEREADBACK_H
#define OUTSHINE_RENDER_SCENE_FRAMEREADBACK_H

#include "FrameBuffer.h"
#include <cstdint>
#include <string>
#include <vector>

namespace outshine::Render {
class SceneRenderer;

[[nodiscard]] bool
ReadFrame(SceneRenderer &renderer, std::vector<uint8_t> &rgba, std::string &error);
[[nodiscard]] bool
ReadFrame(SceneRenderer &renderer, Buffer buffer, std::vector<float> &out, std::string &error);
}
#endif
