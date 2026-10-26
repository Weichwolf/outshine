#include "SceneRenderer.h"
#include "EnvironmentSpecularLayout.h"
#include <expected>
#include <string>
#include <utility>

namespace outshine::Render {

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
      return (resource == Resource::DepthPyramid ? frame.DepthPyramid : frame.GroundPyramid)
          .Allocate(*Device_, {.WidthPx = frame.Width, .HeightPx = frame.Height});
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
