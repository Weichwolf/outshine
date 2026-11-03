#include "Check.h"
#include "ExecutionPlan.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(StorageResource(kNoEdge) == kNoEdge,
        "an absent attachment remains absent without indexing the resource catalogue");
  PlanSpec specification;
  specification.Content = {Stage::OpaqueGeometry};
  specification.Outputs = {Resource::SceneHdr, Resource::SceneDepth};
  const auto compiled = ExecutionPlan::Compile(specification);
  CHECK(compiled, "opaque rendering resolves both phases without a second content switch");
  if (!compiled) { return Report(); }
  const auto &plan = **compiled;
  const auto order = plan.Order();
  const auto position = [&](Stage stage) {
    return std::ranges::find(order, stage) - order.begin();
  };
  CHECK(position(Stage::OpaqueGeometry) < position(Stage::Ground) &&
            position(Stage::Ground) < position(Stage::VisibilityPyramid) &&
            position(Stage::VisibilityPyramid) < position(Stage::GeometryRetryRoots) &&
            position(Stage::GeometryRetryCull) < position(Stage::GeometryRetryCompact) &&
            position(Stage::GeometryRetryCompact) < position(Stage::OpaqueGeometryRetry) &&
            position(Stage::OpaqueGeometryRetry) < position(Stage::DepthPyramid),
        "terrain contributes current occluder depth before recovery and final history");
  CHECK(plan.Stored(Resource::SceneDepth) && plan.Stored(Resource::SceneHdr),
        "phase-one attachments survive the intervening compute work");
  size_t rasterPhases = 0;
  for (const auto &pass : plan.Passes()) {
    const auto stages = std::span(order).subspan(pass.First, pass.Count);
    if (!std::ranges::contains(stages, Stage::OpaqueGeometry) &&
        !std::ranges::contains(stages, Stage::OpaqueGeometryRetry)) {
      continue;
    }
    CHECK(pass.Kind == PassKind::Raster && pass.Depth == Resource::SceneDepth &&
              std::ranges::find(pass.Targets, Resource::SceneHdr) != pass.Targets.end(),
          "both phases use the same scene colour and depth resources");
    ++rasterPhases;
  }
  CHECK(rasterPhases == 2, "opaque meshes have a first raster phase and a distinct recovery phase");
  size_t versions = 0;
  for (size_t at = 0; at < kResourceCount; ++at) {
    const auto &row = Row(static_cast<Resource>(at));
    if (row.Backing == kNoEdge) { continue; }
    const auto &base = Row(row.Backing);
    CHECK(base.Backing == kNoEdge && row.Format == base.Format && row.Stride == base.Stride &&
              StorageResource(row.Id) == base.Id,
          "logical buffer versions reuse a compatible owned allocation");
    ++versions;
  }
  CHECK(versions != 0, "recovery does not require a second complete geometry allocation");
  CHECK(Names(Row(Stage::GeometryRetryCull).Reads, Resource::ClusterFirstKept) &&
            Names(Row(Stage::GeometryRetryCull).Reads, Resource::VisibilityPyramid),
        "the complete recovery dependencies fit beyond the old eight-edge limit");
  return Report();
}
