#include "UploadQueue.h"
#include "OverlayDraw.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
constexpr uint64_t kChannels = 4;

Native::Error InvalidOverlay() {
  return {.Kind = Native::ErrorKind::Invalid,
          .Message = "overlay update requires valid bounded rectangles, device and atlas pixels"};
}

bool ValidQuads(const OverlayQuad *quads, size_t count) {
  return count <= kMaxOverlayQuads && (count == 0 || quads != nullptr);
}

Native::Result<uint64_t> AtlasBytes(const OverlayDraw::AtlasPixels *atlas) {
  if (atlas == nullptr) { return 0; }
  if (atlas->Rgba == nullptr || atlas->Width <= 0 || atlas->Height <= 0) {
    return std::unexpected(InvalidOverlay());
  }
  const uint64_t bytes = uint64_t{static_cast<uint32_t>(atlas->Width)} *
                         static_cast<uint32_t>(atlas->Height) * kChannels;
  if (bytes > std::numeric_limits<uint32_t>::max()) { return std::unexpected(InvalidOverlay()); }
  return bytes;
}

Native::Result<void> UploadOverlay(Native::Device &device,
                                   Native::Commands &commands,
                                   Native::Buffer *vertices,
                                   Native::Texture *texture,
                                   const OverlayQuad *quads,
                                   uint64_t quadBytes,
                                   const OverlayDraw::AtlasPixels *atlas,
                                   uint64_t atlasBytes) {
  auto staging = device.CreateBuffer({.Bytes = quadBytes + atlasBytes,
                                      .Use = Native::BufferUse::CopySource,
                                      .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  auto mapped = device.MapWrite(**staging, {.Bytes = quadBytes + atlasBytes});
  if (!mapped) { return std::unexpected(mapped.error()); }
  if (quadBytes > 0) { std::memcpy(mapped->data(), quads, static_cast<size_t>(quadBytes)); }
  if (atlas != nullptr) {
    std::memcpy(mapped->data() + quadBytes, atlas->Rgba, static_cast<size_t>(atlasBytes));
  }
  auto accepted = device.Unmap(**staging);
  if (!accepted) { return accepted; }
  if (quadBytes > 0) {
    accepted = commands.Copy(**staging, *vertices, {.Bytes = quadBytes});
    if (!accepted) { return accepted; }
  }
  if (atlas != nullptr) {
    accepted = commands.Copy(**staging,
                             *texture,
                             {.BufferOffset = quadBytes,
                              .RowBytes = static_cast<uint32_t>(atlas->Width) * kChannels,
                              .Region = {.Width = static_cast<uint32_t>(atlas->Width),
                                         .Height = static_cast<uint32_t>(atlas->Height)}});
    if (!accepted) { return accepted; }
  }
  return {};
}

}

Native::Result<void> OverlayDraw::EnsureAtlas(const Gpu &gpu) {
  if (Atlas_) {
    if (Device_ != gpu.Device) { return std::unexpected(InvalidOverlay()); }
    return {};
  }
  static constexpr std::array<uint8_t, 4> white{255, 255, 255, 255};
  return SetAtlas(gpu, white.data(), 1, 1);
}

Native::Result<void>
OverlayDraw::SetAtlas(const Gpu &gpu, const uint8_t *rgba, int width, int height) {
  const AtlasPixels atlas{.Rgba = rgba, .Width = width, .Height = height};
  return Update(gpu, nullptr, 0, &atlas, false);
}

Native::Result<void> OverlayDraw::SetQuads(const Gpu &gpu, const OverlayQuad *quads, size_t count) {
  return Update(gpu, quads, count, nullptr, true);
}

Native::Result<void> OverlayDraw::Replace(const Gpu &gpu,
                                          const OverlayQuad *quads,
                                          size_t count,
                                          const AtlasPixels *atlas) {
  return Update(gpu, quads, count, atlas, true);
}

Native::Result<void> OverlayDraw::Update(const Gpu &gpu,
                                         const OverlayQuad *quads,
                                         size_t count,
                                         const AtlasPixels *atlas,
                                         bool changeQuads) {
  if (!ValidQuads(quads, count)) { return std::unexpected(InvalidOverlay()); }
  if (changeQuads && count == 0 && atlas == nullptr) {
    Count = 0;
    return {};
  }
  if (gpu.Device == nullptr || (Device_ != nullptr && Device_ != gpu.Device)) {
    return std::unexpected(InvalidOverlay());
  }
  static constexpr std::array<uint8_t, 4> white{255, 255, 255, 255};
  const AtlasPixels fallback{.Rgba = white.data(), .Width = 1, .Height = 1};
  if (atlas == nullptr && count > 0 && !Atlas_) { atlas = &fallback; }
  const auto atlasBytes = AtlasBytes(atlas);
  if (!atlasBytes) { return std::unexpected(atlasBytes.error()); }
  const uint64_t quadBytes = changeQuads ? count * sizeof(OverlayQuad) : 0;
  auto commands = BeginUpload(*gpu.Device, gpu.Uploads);
  if (!commands) { return std::unexpected(commands.error()); }
  std::unique_ptr<Native::Texture> madeAtlas;
  if (atlas != nullptr) {
    auto made = gpu.Device->CreateTexture({.Width = static_cast<uint32_t>(atlas->Width),
                                           .Height = static_cast<uint32_t>(atlas->Height),
                                           .Texels = Native::Format::Rgba8Unorm,
                                           .Use = Native::TextureUse::Sampled |
                                                  Native::TextureUse::CopyDestination |
                                                  Native::TextureUse::CopySource});
    if (!made) { return std::unexpected(made.error()); }
    madeAtlas = std::move(*made);
  }
  std::unique_ptr<Native::Buffer> madeVertices;
  Native::Buffer *target = SpareVerts.get();
  if (quadBytes > 0 && target == nullptr) {
    auto made = gpu.Device->CreateBuffer({.Bytes = kMaxOverlayQuads * sizeof(OverlayQuad),
                                          .Use = Native::BufferUse::Vertex |
                                                 Native::BufferUse::CopyDestination |
                                                 Native::BufferUse::CopySource});
    if (!made) { return std::unexpected(made.error()); }
    madeVertices = std::move(*made);
    target = madeVertices.get();
  }
  auto uploaded = UploadOverlay(
      *gpu.Device, **commands, target, madeAtlas.get(), quads, quadBytes, atlas, *atlasBytes);
  if (!uploaded) { return uploaded; }
  const auto submitted = SubmitUpload(*gpu.Device, gpu.Uploads, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  if (madeAtlas) { Atlas_ = std::move(madeAtlas); }
  if (madeVertices) { SpareVerts = std::move(madeVertices); }
  if (quadBytes > 0) { std::swap(Verts, SpareVerts); }
  if (changeQuads) { Count = static_cast<uint32_t>(count); }
  Device_ = gpu.Device;
  return {};
}
}
