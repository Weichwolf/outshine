#include "ExecutionPlan.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <span>

namespace {
using namespace outshine::Render;

bool HasTarget(const ExecutionPlan &plan, Stage stage, Resource target) {
  for (const auto &pass : plan.Passes()) {
    if (std::ranges::contains(std::span(plan.Order()).subspan(pass.First, pass.Count), stage)) {
      return std::ranges::contains(pass.Targets, target);
    }
  }
  return false;
}
}

int main() {
  using namespace outshine::Test;
  const auto plain = ExecutionPlan::Compile({.Outputs = {Resource::SceneHdr},
                                             .Content = {Stage::OpaqueGeometry},
                                             .Exposure = {},
                                             .Display = {},
                                             .Precision = {}});
  CHECK(plain && !(**plain).Holds(Resource::GroundFieldRequests) &&
            !(**plain).Holds(Resource::GroundFieldSamples) &&
            !(**plain).Holds(Stage::GroundFieldFeedback),
        "ordinary geometry plans reserve neither feedback images nor compute work");
  const auto cached = ExecutionPlan::Compile({.Outputs = {Resource::SceneHdr,
                                                          Resource::GroundFieldSamples,
                                                          Resource::SceneShadingNormal,
                                                          Resource::SceneSurfaceIdentity,
                                                          Resource::SceneSurfaceBase,
                                                          Resource::SceneSurfaceMetalRough},
                                              .Content = {Stage::OpaqueGeometry},
                                              .Exposure = {},
                                              .Display = {},
                                              .Precision = {}});
  CHECK(cached, "native material capture can explicitly request terrain field feedback");
  if (!cached) { return Report(); }
  const auto &plan = **cached;
  const auto ground = std::ranges::find(plan.Order(), Stage::Ground);
  const auto feedback = std::ranges::find(plan.Order(), Stage::GroundFieldFeedback);
  CHECK(ground != plan.Order().end() && feedback != plan.Order().end() && ground < feedback,
        "sampling reads the completed visible terrain contribution");
  CHECK(plan.Holds(Resource::GroundFieldRequests) && plan.Stored(Resource::GroundFieldRequests),
        "the feedback attachment survives until compute sampling consumes it");
  CHECK(!HasTarget(plan, Stage::OpaqueGeometry, Resource::GroundFieldRequests) &&
            HasTarget(plan, Stage::Ground, Resource::GroundFieldRequests),
        "opaque buildings do not inherit terrain-only feedback attachments");
  CHECK(HasTarget(plan, Stage::Ground, Resource::SceneSurfaceMetalRough),
        "adding feedback preserves the complete terrain material capture");
  CHECK(!ExecutionPlan::Compile({.Outputs = {Resource::GroundFieldSamples},
                                 .Content = {},
                                 .Exposure = {},
                                 .Display = {},
                                 .Precision = {}}),
        "feedback without any terrain producer is rejected instead of silently reading emptiness");
  return Report();
}
