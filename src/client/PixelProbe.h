#ifndef OUTSHINE_CLIENT_PIXELPROBE_H
#define OUTSHINE_CLIENT_PIXELPROBE_H

#include <expected>
#include <string>
#include <string_view>

namespace outshine {
class Engine;

namespace Scenario {
struct Document;
}
}

namespace outshine::Client {

struct PixelCoordinate {
  int X = 0;
  int Y = 0;
};

[[nodiscard]] std::expected<PixelCoordinate, std::string_view>
ParsePixelCoordinate(std::string_view value) noexcept;

void PreparePixelAttachments(Scenario::Document &declared);

[[nodiscard]] std::expected<void, std::string>
ReportPixel(Engine &engine, std::string_view name, PixelCoordinate at);

[[nodiscard]] std::expected<void, std::string> SaveLinearImage(Engine &engine,
                                                               std::string_view path);

}

#endif
