#ifndef OUTSHINE_APP_CAPTURE_IMAGECAPTURE_H
#define OUTSHINE_APP_CAPTURE_IMAGECAPTURE_H

#include "Extent.h"
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace outshine {
class Engine;

namespace Render {
class SceneRenderer;
}

namespace App {
[[nodiscard]] std::expected<void, std::string>
SaveImage(std::span<const uint8_t> rgba, Extent frame, std::string_view path);
[[nodiscard]] std::expected<void, std::string> SaveImage(Engine &engine, std::string_view path);
[[nodiscard]] std::expected<void, std::string>
SaveImage(Render::SceneRenderer &renderer, Extent frame, std::string_view path);
}
}
#endif
