#ifndef OUTSHINE_CLIENT_PIXELPROBE_H
#define OUTSHINE_CLIENT_PIXELPROBE_H

#include <expected>
#include <string>
#include <string_view>

namespace outshine {
class Engine;
}

namespace outshine::Client {

struct PixelCoordinate {
  int X = 0;
  int Y = 0;
};

[[nodiscard]] std::expected<PixelCoordinate, std::string_view>
ParsePixelCoordinate(std::string_view value) noexcept;

[[nodiscard]] std::expected<void, std::string> PreparePixelAttachments(Engine &engine);

[[nodiscard]] std::expected<void, std::string>
ReportPixel(Engine &engine, std::string_view name, PixelCoordinate at);

}

#endif
