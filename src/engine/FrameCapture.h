#ifndef OUTSHINE_ENGINE_FRAMECAPTURE_H
#define OUTSHINE_ENGINE_FRAMECAPTURE_H

#include "Extent.h"
#include "Outshine.h"

#include <string>
#include <string_view>
#include <vector>

namespace outshine::Render {
class SceneRenderer;
}

namespace outshine::Core {

[[nodiscard]] bool
ReadFrame(Render::SceneRenderer &renderer, std::vector<uint8_t> &rgba, std::string &error);
[[nodiscard]] bool ReadFrame(Render::SceneRenderer &renderer,
                             outshine::Buffer buffer,
                             std::vector<float> &out,
                             std::string &error);
[[nodiscard]] bool
SaveFrame(Render::SceneRenderer &renderer, Extent frame, std::string_view path, std::string &error);

}
#endif
