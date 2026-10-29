#include "MaterialFieldInputData.h"
#include "ImageMips.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace outshine::Render {
std::expected<void, std::string> MaterialFieldInput::Data::PrepareTextures() {
  using Entry = std::pair<MaterialTexture MaterialBinding::*, ImageMipKind>;
  constexpr std::array<Entry, 8> maps{
      {{&MaterialBinding::Colour, ImageMipKind::Colour},
       {&MaterialBinding::Normal, ImageMipKind::Normal},
       {&MaterialBinding::MetalRough, ImageMipKind::Linear},
       {&MaterialBinding::Emissive, ImageMipKind::Colour},
       {&MaterialBinding::SpecularStrength, ImageMipKind::Linear},
       {&MaterialBinding::SpecularTint, ImageMipKind::Colour},
       {&MaterialBinding::DiffuseReflection, ImageMipKind::Linear},
       {&MaterialBinding::DiffuseTransmission, ImageMipKind::Linear}}};
  for (auto &material : Materials) {
    for (const auto &[member, kind] : maps) {
      if (auto ready = PrepareTexture(material.*member, kind); !ready) { return ready; }
    }
  }
  return {};
}

std::expected<void, std::string> MaterialFieldInput::Data::PrepareTexture(MaterialTexture &texture,
                                                                          ImageMipKind kind) {
  if (texture.Rgba == nullptr || texture.Mip == MipFilter::None ||
      (texture.Width == 1 && texture.Height == 1)) {
    return {};
  }
  if (texture.Width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      texture.Height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
    return std::unexpected("material field texture exceeds addressing");
  }
  const int width = static_cast<int>(texture.Width);
  const int height = static_cast<int>(texture.Height);
  const auto bytes = LowerImageMipBytes(width, height, texture.Format);
  if (!bytes) { return std::unexpected("material field texture has invalid dimensions"); }
  if (kind == ImageMipKind::Colour && texture.Format != ImagePixelFormat::Rgba8Unorm) {
    kind = ImageMipKind::Linear;
  }
  auto &lower = texture.LowerMips[static_cast<size_t>(kind)];
  if (lower) {
    if (lower->size() != *bytes) {
      return std::unexpected("material field texture has an incomplete mip chain");
    }
    return {};
  }
  auto mip = Core::PrepareImageMips(
      {.WidthPx = width,
       .HeightPx = height,
       .Format = texture.Format,
       .Rgba = {texture.Rgba,
                size_t{texture.Width} * texture.Height * ImagePixelBytes(texture.Format)},
       .LowerMips = texture.LowerMips},
      kind);
  if (!mip || mip->size() != *bytes) {
    return std::unexpected("material field texture mip preparation failed");
  }
  PreparedMips.push_back(std::move(*mip));
  lower = std::span<const uint8_t>(PreparedMips.back());
  return {};
}
}
