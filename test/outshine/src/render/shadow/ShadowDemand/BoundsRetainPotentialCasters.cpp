#include "ShadowDemand.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

void AffineBounds() {
  ShadowDemand demand{.Regions = 1};
  const Box inside{.Min = {{-.1, -.2, .1}}, .Max = {{.3, .4, .9}}};
  CHECK(demand.MayContain(inside), "an intersecting caster remains needed");
  for (size_t axis = 0; axis < 3; ++axis) {
    for (const double shift : {-3., 3.}) {
      auto outside = inside;
      outside.Min[axis] += shift;
      outside.Max[axis] += shift;
      CHECK(!demand.MayContain(outside), "each disjoint clip half-space excludes a caster");
    }
  }
  CHECK(demand.MayContain({.Min = {{1, 0, .5}}, .Max = {{1, .1, .6}}}),
        "touching a clip plane retains the caster");
  CHECK(demand.MayContain({.Min = {{1.000001, 0, .5}}, .Max = {{1.000002, .1, .6}}}),
        "float transform uncertainty retains a nearly touching caster");
  demand.Regions = 2;
  demand.ClipFromScene[1].SetTranslation({{-10, 0, 0}});
  CHECK(demand.MayContain({.Min = {{10, 0, .1}}, .Max = {{10.1, .1, .9}}}),
        "any overlapping region retains the source");
  demand.Regions = 0;
  CHECK(!demand.MayContain(inside), "a declared empty shadow pass needs no caster");
}

void UnknownBounds() {
  const Box outside{.Min = {{10, 10, 10}}, .Max = {{11, 11, 11}}};
  ShadowDemand demand{.Regions = 1};
  CHECK(demand.MayContain({}), "missing bounds never establish occlusion");
  demand.ClipFromScene[0][3] = .1;
  CHECK(demand.MayContain(outside), "a non-affine projection retains unknown work");
  demand.ClipFromScene[0] = Mat4{};
  demand.ClipFromScene[0][0] = std::numeric_limits<double>::infinity();
  CHECK(demand.MayContain(outside), "invalid projection metadata retains the source");
  demand.ClipFromScene[0][0] = std::numeric_limits<double>::max();
  CHECK(demand.MayContain(outside), "interval overflow cannot prove absence");
  demand.ClipFromScene[0] = Mat4{};
  demand.EyeM[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(demand.MayContain(outside), "an invalid frame retains unknown work");
  demand.Regions = demand.ClipFromScene.size() + 1;
  CHECK(demand.MayContain(outside), "invalid region counts cannot discard a caster");
}
}

int main() {
  AffineBounds();
  UnknownBounds();
  return outshine::Test::Report();
}
