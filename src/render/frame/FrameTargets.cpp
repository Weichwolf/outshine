#include "FrameTargets.h"
#include "TextureFormat.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>

namespace outshine::Render {
FrameTargets::Slot FrameTargets::Index(Resource resource) noexcept {
  switch (resource) {
    case Resource::SceneHdr: return Slot::Hdr;
    case Resource::SceneVelocity: return Slot::Velocity;
    case Resource::SceneDepth: return Slot::Depth;
    case Resource::SceneShadingNormal: return Slot::ShadingNormal;
    case Resource::SceneSurfaceIdentity: return Slot::SurfaceIdentity;
    case Resource::SceneSurfaceBase: return Slot::SurfaceBase;
    case Resource::SceneSurfaceMetalRough: return Slot::SurfaceMetalRough;
    case Resource::SceneTransmissive: return Slot::Transmissive;
    case Resource::SceneComposited: return Slot::Composited;
    case Resource::SceneAerial: return Slot::Aerial;
    case Resource::FrameTex: return Slot::Frame;
    case Resource::SceneLinear: return Slot::Linear;
    default: return Slot::Count;
  }
}

bool FrameTargets::Owns(Resource resource) noexcept {
  return Index(resource) != Slot::Count;
}

Native::Texture *FrameTargets::Get(Resource resource, size_t linearIndex) const noexcept {
  const Slot slot = Index(resource);
  if (slot == Slot::Count) { return nullptr; }
  const auto index = static_cast<size_t>(slot);
  if (resource == Resource::SceneLinear) {
    assert(linearIndex < 2);
    return Images_[index + linearIndex].get();
  }
  return Images_[index].get();
}

size_t FrameTargets::Count() const noexcept {
  return static_cast<size_t>(
      std::ranges::count_if(Images_, [](const auto &image) { return static_cast<bool>(image); }));
}

Native::Result<void>
FrameTargets::Replace(Native::Device &device, Extent extent, const ExecutionPlan &plan) {
  if (extent.WidthPx <= 0 || extent.HeightPx <= 0) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "frame target dimensions must be positive"});
  }
  FrameTargets candidate;
  for (size_t at = 0; at < kResourceCount; ++at) {
    const auto resource = static_cast<Resource>(at);
    if (!Owns(resource) || !plan.Holds(resource)) { continue; }
    const auto use = resource == Resource::SceneDepth ? Native::TextureUse::DepthAttachment
                                                      : Native::TextureUse::ColourAttachment;
    const size_t count = resource == Resource::SceneLinear ? 2 : 1;
    for (size_t backing = 0; backing < count; ++backing) {
      auto image = device.CreateTexture(
          {.Width = static_cast<uint32_t>(extent.WidthPx),
           .Height = static_cast<uint32_t>(extent.HeightPx),
           .Texels = TextureFormatOf(plan.Format(resource)),
           .Use = use | Native::TextureUse::Sampled | Native::TextureUse::CopySource});
      if (!image) { return std::unexpected(std::move(image.error())); }
      if (!*image) {
        return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                             .Message = "native device returned no frame texture"});
      }
      candidate.Images_[static_cast<size_t>(Index(resource)) + backing] = std::move(*image);
    }
  }
  *this = std::move(candidate);
  return {};
}
}
