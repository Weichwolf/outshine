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

Native::Result<void> Keep(Native::Result<std::unique_ptr<Native::Buffer>> made,
                          std::unique_ptr<Native::Buffer> &into) {
  if (!made) { return std::unexpected(made.error()); }
  into = std::move(*made);
  return {};
}
}

Native::Result<void> SceneRenderer::CreateSampler(FrameResources &frame) {
  auto made = Device_->CreateSampler({.Min = Native::Filter::Linear,
                                      .Mag = Native::Filter::Linear,
                                      .Mip = Native::Filter::Linear});
  if (!made) { return std::unexpected(made.error()); }
  frame.Samp = std::move(*made);
  return {};
}

Native::Result<void>
SceneRenderer::Create(FrameResources &frame, const ExecutionPlan &plan, Resource resource) {
  if (AtmospherePasses::Owns(resource)) {
    return frame.Atmosphere.Create(*Device_, plan, resource);
  }
  switch (resource) {
    case Resource::LinearSampler: return CreateSampler(frame);
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
    case Resource::ShadowAtlas: return frame.Shadow.Allocate(*Device_);
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
