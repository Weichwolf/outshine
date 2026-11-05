#ifndef OUTSHINE_APP_GEOMETRYSNAPSHOTOUTPUT_H
#define OUTSHINE_APP_GEOMETRYSNAPSHOTOUTPUT_H
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace outshine {
class Renderer;

namespace App {
[[nodiscard]] std::expected<void, std::string> WriteGeometrySnapshot(
    Renderer &renderer, const std::filesystem::path &root, std::string_view report);
}
}
#endif
