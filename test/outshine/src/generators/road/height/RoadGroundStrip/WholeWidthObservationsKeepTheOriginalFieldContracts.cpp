#include "Check.h"
#include "CppAllocationProbe.h"
#include "RoadGroundStrip.h"
#include "RoadPortFieldFit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr std::array<LongitudeLatitude, 3> kAxis{
    {{}, {.LongitudeDeg = .00045}, {.LongitudeDeg = .0009}}};

void CheckField() {
  const auto strip = PrepareRoadGroundStrip(kAxis, 2, .05);
  CHECK(strip.has_value(), "a complete bounded source axis prepares its full-width observations");
  if (!strip) { return; }
  std::vector<double> heights;
  for (const auto at : strip->TerrainQueries) {
    const double u = at.LongitudeDeg / kAxis.back().LongitudeDeg;
    heights.push_back(12 + 2 * std::min(u, 1 - u));
  }
  const auto field = BindRoadGroundField(*strip, heights);
  CHECK(field && field->Priors.size() == heights.size() && field->Patches.size() > 1,
        "the original observations remain the height objective rather than being recooked");
  if (!field) { return; }
  const auto fitted = FitRoadPortField(field->FitInput(.1), {});
  CHECK(
      fitted && fitted->MaximumPriorErrorM <= .1 && fitted->MaximumNormalizedGrade <= 1,
      "the assembled non-affine field preserves its original prior and continuous physical limits");
  if (!fitted) { return; }
  for (size_t patch = 1; patch < field->Patches.size(); patch += 2) {
    for (size_t end = 0; end < 2; ++end) {
      const auto at = field->Patches[patch].CutCentres[end];
      const auto road = fitted->Field.Evaluate(patch, at);
      const auto plane = fitted->Field.Evaluate(end == 0 ? patch - 1 : patch + 1, at);
      CHECK(road && plane && std::abs(road->HeightM - plane->HeightM) < 1e-8 &&
                std::abs(road->Gradient.SlopeE - plane->Gradient.SlopeE) < 1e-8 &&
                std::abs(road->Gradient.SlopeN - plane->Gradient.SlopeN) < 1e-8,
            "every constructed shared cut retains the whole C1 plane");
    }
  }
  const auto missing = BindRoadGroundField(*strip, std::span(heights).first(heights.size() - 1));
  CHECK(!missing && missing.error() == RoadGroundError::MissingTerrain,
        "missing full-width observations never become invented zero heights");
  heights[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!BindRoadGroundField(*strip, heights), "nonfinite delivered terrain is refused");
  auto malformed = *strip;
  malformed.Triangles[0][0] = 99999;
  CHECK(!BindRoadGroundField(malformed, std::vector<double>(heights.size(), 0)),
        "malformed original connectivity cannot reach field layout indexing");
}

void CheckBoundaries() {
  BeginCppAllocationProbe();
  const auto refused = PrepareRoadGroundStrip(kAxis, 2, .05, {.ContainerBytes = 0});
  const auto allocation = EndCppAllocationProbe();
  CHECK(!refused && refused.error() == RoadGroundError::CapacityExceeded && allocation.Calls == 0,
        "initial storage admission precedes axis/polygon/query allocation");
  const auto limited = PrepareRoadGroundStrip(kAxis, 2, .05, {.TerrainSamples = 1});
  CHECK(!limited && limited.error() == RoadGroundError::CapacityExceeded,
        "terrain demand is admitted before any terrain consumer can be called");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = PrepareRoadGroundStrip(kAxis, 2, .05, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == RoadGroundError::Canceled,
        "canceled source geometry cannot publish a terrain request");
  const std::array<LongitudeLatitude, 2> collapsed{{{}, {}}};
  CHECK(!PrepareRoadGroundStrip(collapsed, 2, .05), "collapsed source axes stay unresolved");
  const std::array<LongitudeLatitude, 3> turn{
      {{}, {.LongitudeDeg = .001}, {.LongitudeDeg = .001, .LatitudeDeg = .001}}};
  CHECK(!PrepareRoadGroundStrip(turn, 2, .05),
        "turns beyond this coarse recipe are explicit failures");
}

void CheckPlaneMerging() {
  const std::array<LongitudeLatitude, 3> tiny{
      {{}, {.LongitudeDeg = 9e-11}, {.LongitudeDeg = .0009}}};
  const auto strip = PrepareRoadGroundStrip(tiny, 2, .05);
  CHECK(strip.has_value(),
        "a narrow original control interval remains an explicit bounded source strip");
  if (!strip) { return; }
  const auto field =
      BindRoadGroundField(*strip, std::vector<double>(strip->TerrainQueries.size(), 42));
  CHECK(field && field->Work.MergedPlanes > 0,
        "ill-conditioned tiny transition bands share their original parent plane");
  if (!field) { return; }
  CHECK(FitRoadPortField(field->FitInput(.1), {}).has_value(),
        "the reference's shared-plane fallback preserves a complete valid field");
}
}

int main() {
  CheckField();
  CheckBoundaries();
  CheckPlaneMerging();
  return Report();
}
