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

Native::Result<void> SceneRenderer::CreateAtmosphereLut(FrameResources &frame,
                                                        const ExecutionPlan &plan,
                                                        Resource resource) {
  struct Shape {
    uint32_t Width, Height;
    std::unique_ptr<Native::Texture> *Into;
  };

  const Shape shape = [&frame, resource] -> Shape {
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
                              .Texels = TextureFormatOf(plan.Format(resource)),
                              .Use = Native::TextureUse::Storage | Native::TextureUse::Sampled |
                                     Native::TextureUse::CopySource}),
      *shape.Into);
}

Native::Result<void>
SceneRenderer::Create(FrameResources &frame, const ExecutionPlan &plan, Resource resource) {
  using enum Native::TextureUse;
  switch (resource) {
    case Resource::LinearSampler:
    case Resource::LutSampler: return CreateSampler(frame, resource);
    case Resource::TransmittanceLut:
    case Resource::MultiScatterLut:
    case Resource::SkyViewLut:
    case Resource::EnvironmentSpecular: return CreateAtmosphereLut(frame, plan, resource);
    case Resource::IrradianceBuffer:
      return Keep(Device_->CreateBuffer(
                      {.Bytes = kIrradianceFloats * sizeof(float),
                       .Use = Native::BufferUse::Storage | Native::BufferUse::CopySource}),
                  frame.IrradianceBuffer);
    case Resource::GroundPyramid:
    case Resource::DepthPyramid:
      return Keep(
          Device_->CreateBuffer(
              {.Bytes = uint64_t{PyramidOver({.WidthPx = static_cast<uint32_t>(frame.Width),
                                              .HeightPx = static_cast<uint32_t>(frame.Height)})
                                     .Texels} *
                        sizeof(float),
               .Use = Native::BufferUse::Storage | Native::BufferUse::CopySource}),
          resource == Resource::DepthPyramid ? frame.Pyramid : frame.GroundPyramid);
    case Resource::ShadowAtlas:
      return Keep(Device_->CreateTexture({.Width = kShadowAtlasPx,
                                          .Height = kShadowAtlasPx,
                                          .Texels = Native::Format::Depth32Float,
                                          .Use = DepthAttachment | Sampled | CopySource}),
                  frame.ShadowAtlas);
    case Resource::AtmosphereUniform:
    case Resource::CascadeUniform:
    case Resource::VegetationTable:
    case Resource::Meter:
    case Resource::OverlayAtlas:
    case Resource::Surface:
    case Resource::GroundSelection:
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
    default:
      if (!FrameTargets::Owns(resource) || frame.Targets.Get(resource) != nullptr) { return {}; }
      return frame.Targets.Replace(
          *Device_, {.WidthPx = frame.Width, .HeightPx = frame.Height}, plan);
  }
}

bool SceneRenderer::Accepted(Native::Result<void> result, std::string &error) {
  if (!result) {
    error = std::move(result.error().Message);
    return false;
  }
  return true;
}
}
