#include "PixelProbe.h"

#include <Outshine.h>
#include <scenario/Scenario.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace outshine::Client {
namespace {

[[nodiscard]] std::expected<int, std::string_view>
NonnegativeInteger(std::string_view value) noexcept {
  int number = 0;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
  if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
      number < 0) {
    return std::unexpected("--probe-pixel requires nonnegative integer x,y");
  }
  return number;
}

[[nodiscard]] std::expected<std::array<float, 4>, std::string>
ReadFloatPixel(Renderer renderer,
               Buffer buffer,
               std::string_view label,
               size_t index,
               size_t pixels,
               size_t channels) {
  std::vector<float> values;
  if (const auto read = renderer.readPixels(buffer, values); !read) {
    return std::unexpected(std::string(label) + ": " + read.error());
  }
  if (values.size() != pixels * channels || index >= pixels) {
    return std::unexpected(std::string(label) + ": unexpected attachment extent");
  }
  std::array<float, 4> pixel{};
  for (size_t channel = 0; channel < channels; ++channel) {
    pixel[channel] = values[index * channels + channel];
    if (!std::isfinite(pixel[channel])) {
      return std::unexpected(std::string(label) + ": nonfinite pixel component");
    }
  }
  return pixel;
}

}

std::expected<PixelCoordinate, std::string_view>
ParsePixelCoordinate(std::string_view value) noexcept {
  const size_t comma = value.find(',');
  if (comma == std::string_view::npos) {
    return std::unexpected("--probe-pixel requires nonnegative integer x,y");
  }
  std::string_view xPart = value;
  std::string_view yPart = value;
  xPart.remove_suffix(value.size() - comma);
  yPart.remove_prefix(comma + 1);
  const auto x = NonnegativeInteger(xPart);
  const auto y = NonnegativeInteger(yPart);
  if (!x) { return std::unexpected(x.error()); }
  if (!y) { return std::unexpected(y.error()); }
  return PixelCoordinate{.X = *x, .Y = *y};
}

std::expected<void, std::string> PreparePixelAttachments(Engine &engine) {
  Scenario::Document declared = engine.declaration();
  constexpr std::array<std::string_view, 2> attachments{"sceneShadingNormal",
                                                        "sceneSurfaceIdentity"};
  for (const std::string_view attachment : attachments) {
    if (std::ranges::find(declared.Render.Outputs, attachment) == declared.Render.Outputs.end()) {
      declared.Render.Outputs.emplace_back(attachment);
    }
  }
  return engine.declare(declared);
}

std::expected<void, std::string>
ReportPixel(Engine &engine, std::string_view name, PixelCoordinate at) {
  if (engine.declaration().Ground.Declared && !engine.settled(WorldQuality::Refined)) {
    return std::unexpected("probe requires a refined published world");
  }
  const Extent extent = engine.swapChain().extent();
  if (at.X < 0 || at.Y < 0 || at.X >= extent.WidthPx || at.Y >= extent.HeightPx) {
    return std::unexpected("probe pixel lies outside the current render target");
  }
  const auto width = static_cast<size_t>(extent.WidthPx);
  const auto height = static_cast<size_t>(extent.HeightPx);
  if (width > std::numeric_limits<size_t>::max() / height / 4u) {
    return std::unexpected("probe target extent exceeds addressable pixel storage");
  }
  const size_t pixels = width * height;
  const size_t index = static_cast<size_t>(at.Y) * width + static_cast<size_t>(at.X);
  Renderer renderer = engine.renderer();
  std::vector<uint8_t> rgba;
  if (const auto read = renderer.readPixels(rgba); !read) {
    return std::unexpected("colour: " + read.error());
  }
  if (rgba.size() != pixels * 4u) {
    return std::unexpected("colour: unexpected attachment extent");
  }
  const std::array colour{
      rgba[index * 4u], rgba[index * 4u + 1u], rgba[index * 4u + 2u], rgba[index * 4u + 3u]};
  const auto linear = ReadFloatPixel(renderer, Buffer::Linear, "linear", index, pixels, 4u);
  if (!linear) { return std::unexpected(linear.error()); }
  const auto depth = ReadFloatPixel(renderer, Buffer::Depth, "depth", index, pixels, 1u);
  if (!depth) { return std::unexpected(depth.error()); }
  const auto normal = ReadFloatPixel(renderer, Buffer::ShadingNormal, "normal", index, pixels, 4u);
  if (!normal) { return std::unexpected(normal.error()); }
  const auto identity =
      ReadFloatPixel(renderer, Buffer::SurfaceIdentity, "identity", index, pixels, 4u);
  if (!identity) { return std::unexpected(identity.error()); }
  std::println("PIXEL\t{}\t{}\t{}\t{},{},{},{}\t{:.9g},{:.9g},{:.9g}\t{:.9g}\t{:.9g},{:.9g},{:.9g}"
               "\t{:.9g}\t{}",
               name,
               at.X,
               at.Y,
               static_cast<unsigned>(colour[0]),
               static_cast<unsigned>(colour[1]),
               static_cast<unsigned>(colour[2]),
               static_cast<unsigned>(colour[3]),
               (*linear)[0],
               (*linear)[1],
               (*linear)[2],
               (*depth)[0],
               (*normal)[0],
               (*normal)[1],
               (*normal)[2],
               (*identity)[0],
               engine.declaration().Ground.Declared ? "refined" : "groundless");
  return {};
}

}
