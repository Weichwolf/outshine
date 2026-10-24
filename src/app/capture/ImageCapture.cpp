#include "ImageCapture.h"
#include "Engine.h"
#include "FrameReadback.h"
#include "Image.h"
#include "WriteFileAtomically.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace outshine::App {
std::expected<void, std::string>
SaveImage(std::span<const uint8_t> rgba, Extent frame, std::string_view path) {
  if (frame.WidthPx <= 0 || frame.HeightPx <= 0 ||
      static_cast<size_t>(frame.HeightPx) >
          std::numeric_limits<size_t>::max() / 4u / static_cast<size_t>(frame.WidthPx) ||
      rgba.size() !=
          static_cast<size_t>(frame.WidthPx) * static_cast<size_t>(frame.HeightPx) * 4u) {
    return std::unexpected("image capture requires a complete RGBA8 frame");
  }
  if (path.empty() || path.contains('\0')) {
    return std::unexpected("image capture requires a valid output path");
  }
  std::vector<uint8_t> png;
  if (!Core::EncodePng(rgba.data(), frame.WidthPx, frame.HeightPx, png)) {
    return std::unexpected("the frame did not encode as a png");
  }
  const std::filesystem::path destination(path);
  if (destination.has_parent_path()) {
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error) { return std::unexpected("image capture directory: " + error.message()); }
  }
  return WriteFileAtomically(path, std::as_bytes(std::span(png)));
}

std::expected<void, std::string> SaveImage(Engine &engine, std::string_view path) {
  std::vector<uint8_t> rgba;
  if (const auto read = engine.renderer().readPixels(rgba); !read) { return read; }
  return SaveImage(rgba, engine.swapChain().extent(), path);
}

std::expected<void, std::string>
SaveImage(Render::SceneRenderer &renderer, Extent frame, std::string_view path) {
  std::vector<uint8_t> rgba;
  std::string error;
  if (!Render::ReadFrame(renderer, rgba, error)) { return std::unexpected(error); }
  return SaveImage(rgba, frame, path);
}
}
