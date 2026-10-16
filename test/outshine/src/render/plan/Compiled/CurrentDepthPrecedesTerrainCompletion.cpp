#include "Compiled.h"
#include "Check.h"
#include <algorithm>
#include <iterator>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(AccumulatedAttachmentHasEarlierProducer(Stage::GroundPyramid, Resource::SceneDepth),
        "current attachment depth has preceding opaque producers");
  CHECK(!AccumulatedAttachmentHasEarlierProducer(Stage::SubjectCull, Resource::SceneDepth),
        "an attachment read before its first contributor is invalid");
  CHECK(!AccumulatedAttachmentHasEarlierProducer(Stage::GroundPyramid, Resource::DepthPyramid),
        "accumulated reads cannot hide derived-resource dependency cycles");
  const auto compiled =
      Compiled::Compile({.Outputs = {Resource::SceneHdr}, .Content = {Stage::Subjects}});
  CHECK(compiled, "the existing subjects declaration includes terrain and its current visibility");
  if (!compiled) { return Report(); }
  const auto &plan = **compiled;
  const auto at = [&plan](Stage stage) {
    return std::distance(plan.Order().begin(), std::ranges::find(plan.Order(), stage));
  };
  CHECK(at(Stage::Subjects) < at(Stage::GroundPyramid) &&
            at(Stage::GroundPyramid) < at(Stage::GroundSelection) &&
            at(Stage::GroundSelection) < at(Stage::Ground),
        "opaque draw, current depth, selection and terrain have distinct ordered phases");
  CHECK(plan.Stored(Resource::SceneDepth),
        "depth survives the read between its raster contributors");
  CHECK(!Compiled::Compile({.Outputs = {Resource::GroundPyramid}}),
        "current depth cannot be read without an active preceding producer");
  return Report();
}
