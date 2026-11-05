#include "ExecutionPlan.h"
#include "Check.h"
#include <algorithm>
#include <iterator>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(AccumulatedAttachmentHasEarlierProducer(Stage::GroundPyramid, Resource::SceneDepth),
        "current attachment depth has preceding opaque producers");
  CHECK(!AccumulatedAttachmentHasEarlierProducer(Stage::GeometryCull, Resource::SceneDepth),
        "an attachment read before its first contributor is invalid");
  CHECK(!AccumulatedAttachmentHasEarlierProducer(Stage::GroundPyramid, Resource::DepthPyramid),
        "accumulated reads cannot hide derived-resource dependency cycles");
  const auto compiled = ExecutionPlan::Compile({.Outputs = {Resource::SceneHdr},
                                                .Content = {Stage::OpaqueGeometry},
                                                .Exposure = {},
                                                .Display = {},
                                                .Precision = {}});
  CHECK(compiled, "the existing geometry declaration includes terrain and its current visibility");
  if (!compiled) { return Report(); }
  const auto &plan = **compiled;
  const auto at = [&plan](Stage stage) {
    return std::distance(plan.Order().begin(), std::ranges::find(plan.Order(), stage));
  };
  CHECK(at(Stage::OccluderDepth) < at(Stage::OccluderPyramid) &&
            at(Stage::OccluderPyramid) < at(Stage::GeometryCull) &&
            at(Stage::GeometryCompact) < at(Stage::OpaqueGeometry) &&
            plan.Stored(Resource::OccluderDepth),
        "current-frame occluder depth precedes selection independently of scene depth");
  CHECK(Names(Row(Stage::GeometryCull).Reads, Resource::OccluderPyramid) &&
            !Names(Row(Stage::GeometryCull).ReadsLastFrame, Resource::DepthPyramid) &&
            Names(Row(Stage::OccluderDepth).ReadsLastFrame, Resource::DrawIndex),
        "only the seed geometry is historical; its current raster depth drives culling");
  CHECK(at(Stage::OpaqueGeometry) < at(Stage::GroundPyramid) &&
            at(Stage::GroundPyramid) < at(Stage::GroundSelection) &&
            at(Stage::GroundSelection) < at(Stage::Ground),
        "opaque draw, current depth, selection and terrain have distinct ordered phases");
  CHECK(plan.Stored(Resource::SceneDepth),
        "depth survives the read between its raster contributors");
  CHECK(!ExecutionPlan::Compile({.Outputs = {Resource::GroundPyramid},
                                 .Content = {},
                                 .Exposure = {},
                                 .Display = {},
                                 .Precision = {}}),
        "current depth cannot be read without an active preceding producer");
  return Report();
}
