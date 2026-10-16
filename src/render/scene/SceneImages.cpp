#include "SceneRenderer.h"
#include "DepthPyramid.h"
#include "EnvironmentSpecularLayout.h"
#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>

namespace outshine::Render {
namespace {
Native::Format ImageFormat(TexelFormat declared) {
  using enum Native::Format;
  switch (declared) {
    case TexelFormat::Rgba16Float: return Rgba16Float;
    case TexelFormat::Rgba32Float: return Rgba32Float;
    case TexelFormat::Rg16Float: return Rg16Float;
    case TexelFormat::R8Unorm: return R8Unorm;
    case TexelFormat::Rgba8UnormSrgb: return Rgba8Srgb;
    case TexelFormat::Depth32Float: return Depth32Float;
    case TexelFormat::Handle:
    case TexelFormat::Table: return Count;
  }
  return Count;
}

Native::Result<void> Keep(Native::Result<std::unique_ptr<Native::Texture>> made,
                          std::unique_ptr<Native::Texture> &into) {
  if (!made) { return std::unexpected(made.error()); }
  into = std::move(*made);
  return {};
}

Native::Result<void> Keep(Native::Result<std::unique_ptr<Native::Buffer>> made,
                          std::unique_ptr<Native::Buffer> &into) {
  if (!made) { return std::unexpected(made.error()); }
  into = std::move(*made);
  return {};
}
}

Native::Result<void> SceneRenderer::CreateSampler(FrameResources &frame, Resource resource) {
  auto made = Device_->CreateSampler({.Min = Native::Filter::Linear,
                                      .Mag = Native::Filter::Linear,
                                      .Mip = resource == Resource::LinearSampler
                                                 ? Native::Filter::Linear
                                                 : Native::Filter::Nearest});
  if (!made) { return std::unexpected(made.error()); }
  (resource == Resource::LinearSampler ? frame.Samp : frame.LutSamp) = std::move(*made);
  return {};
}

Native::Result<void>
SceneRenderer::CreateAtmosphereLut(FrameResources &frame, const Compiled &plan, Resource resource) {
  struct Shape {
    uint32_t Width, Height;
    std::unique_ptr<Native::Texture> *Into;
  };

  const Shape shape = [&frame, resource]() -> Shape {
    switch (resource) {
      case Resource::MultiScatterLut:
        return {.Width = kMultiScatterLutSize,
                .Height = kMultiScatterLutSize,
                .Into = &frame.MultiScatterLut};
      case Resource::EnvironmentSpecular:
        return {.Width = kEnvironmentWidth,
                .Height = kEnvironmentHeight,
                .Into = &frame.EnvironmentSpecular};
      case Resource::SkyViewLut:
        return {.Width = kSkyViewLutWidth, .Height = kSkyViewLutHeight, .Into = &frame.SkyViewLut};
      default:
        return {.Width = kTransmittanceLutWidth,
                .Height = kTransmittanceLutHeight,
                .Into = &frame.TransmittanceLut};
    }
  }();
  return Keep(
      Device_->CreateTexture({.Width = shape.Width,
                              .Height = shape.Height,
                              .Texels = ImageFormat(plan.Format(resource)),
                              .Use = Native::TextureUse::Storage | Native::TextureUse::Sampled |
                                     Native::TextureUse::CopySource}),
      *shape.Into);
}

Native::Result<void>
SceneRenderer::Create(FrameResources &frame, const Compiled &plan, Resource resource) {
  using enum Native::TextureUse;
  const auto target = [&](Resource of,
                          Native::UseMask<Native::TextureUse> use,
                          std::unique_ptr<Native::Texture> &into) {
    return Keep(Device_->CreateTexture({.Width = static_cast<uint32_t>(frame.Width),
                                        .Height = static_cast<uint32_t>(frame.Height),
                                        .Texels = ImageFormat(plan.Format(of)),
                                        .Use = use | CopySource}),
                into);
  };
  const auto colour = ColourAttachment | Sampled;
  switch (resource) {
    case Resource::LinearSampler:
    case Resource::LutSampler: return CreateSampler(frame, resource);
    case Resource::SceneHdr: return target(resource, colour, frame.HdrTex);
    case Resource::SceneTransmissive: return target(resource, colour, frame.TransmissiveTex);
    case Resource::SceneComposited: return target(resource, colour, frame.CompositedTex);
    case Resource::SceneAerial: return target(resource, colour, frame.AerialTex);
    case Resource::SceneVelocity: return target(resource, colour, frame.VelTex);
    case Resource::SceneShadingNormal: return target(resource, colour, frame.ShadingNormalTex);
    case Resource::SceneSurfaceIdentity: return target(resource, colour, frame.SurfaceIdentityTex);
    case Resource::SceneSurfaceBase: return target(resource, colour, frame.SurfaceBaseTex);
    case Resource::SceneSurfaceMetalRough:
      return target(resource, colour, frame.SurfaceMetalRoughTex);
    case Resource::SceneDepth: return target(resource, DepthAttachment | Sampled, frame.DepthTex);
    case Resource::FrameTex: return target(resource, colour, frame.FrameTex);
    case Resource::TransmittanceLut:
    case Resource::MultiScatterLut:
    case Resource::SkyViewLut:
    case Resource::EnvironmentSpecular: return CreateAtmosphereLut(frame, plan, resource);
    case Resource::IrradianceBuffer:
      return Keep(Device_->CreateBuffer(
                      {.Bytes = kIrradianceFloats * sizeof(float),
                       .Use = Native::BufferUse::Storage | Native::BufferUse::CopySource}),
                  frame.IrradianceBuffer);
    case Resource::DepthPyramid:
      return Keep(
          Device_->CreateBuffer(
              {.Bytes = uint64_t{PyramidOver({.WidthPx = static_cast<uint32_t>(frame.Width),
                                              .HeightPx = static_cast<uint32_t>(frame.Height)})
                                     .Texels} *
                        sizeof(float),
               .Use = Native::BufferUse::Storage | Native::BufferUse::CopySource}),
          frame.Pyramid);
    case Resource::ShadowAtlas:
      return Keep(Device_->CreateTexture({.Width = kShadowAtlasPx,
                                          .Height = kShadowAtlasPx,
                                          .Texels = Native::Format::Depth32Float,
                                          .Use = DepthAttachment | Sampled | CopySource}),
                  frame.ShadowAtlas);
    case Resource::SceneLinear: {
      auto made = target(resource, colour, frame.LinearTex[0]);
      if (!made) { return made; }
      made = target(resource, colour, frame.LinearTex[1]);
      if (!made) { return made; }
      frame.LinearAt = 0;
      frame.HistoryHeld = false;
      return {};
    }
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::Meter:
    case Resource::OverlayAtlas:
    case Resource::Surface:
    case Resource::ClusterSphere:
    case Resource::ClusterIndex:
    case Resource::ClusterJobs:
    case Resource::ClusterBatches:
    case Resource::ClusterKept:
    case Resource::ClusterSlot:
    case Resource::DrawIndex:
    case Resource::DrawArguments:
    case Resource::AoBuffer:
    case Resource::kCount: return {};
  }
  return {};
}

bool SceneRenderer::Accepted(Native::Result<void> result, std::string &error) {
  if (!result) {
    error = std::move(result.error().Message);
    return false;
  }
  return true;
}
}
