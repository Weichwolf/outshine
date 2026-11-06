#include "RoadPageMeshProducer.h"
#include "ProfiledRoadMesher.h"
#include "HeightSolid.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

std::shared_ptr<HeightSolid> StepBody() {
  const std::array<PlanarPoint, 3> points{{{-1, -1}, {1, -1}, {0, 1}}};
  const std::array<PlanarFace, 1> faces{{{0, 1, 2}}};
  const std::array<std::array<double, 3>, 1> tops{{{12, 12, 12}}};
  const std::array<double, 3> bottoms{11, 11, 11};
  auto solid = BuildHeightSolid(
      {.Points = points, .Faces = faces, .TopHeights = tops, .BottomHeights = bottoms});
  if (!solid) { return {}; }
  auto body = std::make_shared<HeightSolid>();
  *body = std::move(*solid);
  return body;
}

RoadPageRecipe Recipe(const std::shared_ptr<HeightSolid> &body) {
  RoadPageRecipe recipe;
  for (uint32_t index = 0; index < 3; ++index) {
    recipe.Intervals.push_back(
        {.Edge = index,
         .Lane = index,
         .Stations = {{.EastM = 0}, {.EastM = 1024}},
         .Surface = {.HalfWidthM = 3,
                     .Profile = RoadProfile::Simple,
                     .WearsLinear = {{0.2f, 0.3f, 0.4f}},
                     .Crossfall = 0.025,
                     .Form = index == 0 ? RibbonForm::ClosedShell : RibbonForm::Surface},
         .Body = index == 2 ? body : nullptr});
  }
  return recipe;
}

template <class T> bool Same(std::span<const T> first, std::span<const T> second) {
  return std::ranges::equal(std::as_bytes(first), std::as_bytes(second));
}

void CheckBodies(const RoadMeshBuffers &mixed,
                 const RoadMeshBuffers &closed,
                 const RoadMeshBuffers &step) {
  CHECK(Same<float>(std::span(mixed.PositionM).first(closed.PositionM.size()), closed.PositionM) &&
            Same<float>(std::span(mixed.NormalM).first(closed.NormalM.size()), closed.NormalM) &&
            Same<float>(std::span(mixed.ColourRgba).first(closed.ColourRgba.size()),
                        closed.ColourRgba) &&
            Same<uint32_t>(std::span(mixed.Index).first(closed.Index.size()), closed.Index),
        "closed-shell positions, normals, colours, caps and indices stay byte-identical");
  CHECK(Same<float>(std::span(mixed.PositionM).last(step.PositionM.size()), step.PositionM) &&
            Same<float>(std::span(mixed.NormalM).last(step.NormalM.size()), step.NormalM) &&
            Same<float>(std::span(mixed.ColourRgba).last(step.ColourRgba.size()), step.ColourRgba),
        "the original step body retains every native attribute word");
  const size_t offset = (mixed.PositionM.size() - step.PositionM.size()) / 3;
  const auto tail = std::span(mixed.Index).last(step.Index.size());
  for (size_t at = 0; at < tail.size(); ++at) {
    CHECK(tail[at] == step.Index[at] + offset,
          "step indices differ only by the appended vertex base");
  }
}
}

int main() {
  const auto body = StepBody();
  CHECK(body != nullptr, "the native step-body control is complete");
  if (!body) { return Report(); }
  const auto recipe = Recipe(body);
  RoadPageRequest request{.Address = {.PlanKey = std::string(64, 'a'),
                                      .MaterialKey = std::string(64, 'b'),
                                      .MaterialIndex = 0,
                                      .Anchor = {.LongitudeDeg = 8, .LatitudeDeg = 49},
                                      .Level = 0,
                                      .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 0},
                                                {.Kind = RoadPagePartKind::Interval, .Index = 1},
                                                {.Kind = RoadPagePartKind::Interval, .Index = 2}},
                                      .PositionErrorM = 1,
                                      .DisplayPolicy = RoadDisplayPolicy::UpperWithOriginalBodies},
                          .Parent = {},
                          .RequestKey = {},
                          .NativeBytesMost = size_t{1024} * 1024,
                          .SweepLimits =
                              RoadDisplayLimits{.StationsMost = 8, .PoseQueriesMost = 256}};
  const RoadPageMeshProducer producer(RoadPageKey(request.Address), recipe);
  const auto mixed = producer.Produce(request, {});
  const auto closed =
      MeshRoadPage(ProfiledRoadMesher{}, {.Intervals = {recipe.Intervals[0]}, .Junctions = {}});
  const auto step =
      MeshRoadPage(ProfiledRoadMesher{}, {.Intervals = {recipe.Intervals[2]}, .Junctions = {}});
  CHECK(mixed && closed && step && mixed->Diagnostics.empty() && mixed->Swept.Refused == 0,
        "one complete page contains directly sampled surfaces and both original body kinds");
  if (!mixed || !closed || !step) { return Report(); }
  CHECK(mixed->Mesh.Index.size() == closed->Mesh.Index.size() + step->Mesh.Index.size() + 18,
        "only the open surface becomes six triangles; no fine surface is built and reduced");
  CheckBodies(mixed->Mesh, closed->Mesh, step->Mesh);
  const auto bytes = EncodeRoadPageAsset({.Address = request.Address, .Geometry = *mixed},
                                         request.NativeBytesMost);
  const auto decoded = bytes ? DecodeRoadPageAsset(*bytes, request.NativeBytesMost) : std::nullopt;
  CHECK(decoded && decoded->Address.DisplayPolicy == request.Address.DisplayPolicy,
        "native persistence retains the explicit mixed policy");
  request.SweepLimits = RoadDisplayLimits{.StationsMost = 1, .PoseQueriesMost = 256};
  CHECK(!producer.Produce(request, {}), "failed surface coverage publishes no mixed partial page");
  request.Address.DisplayPolicy = RoadDisplayPolicy::UpperOnly;
  CHECK(!producer.Produce(request, {}), "another policy cannot alias the mixed page identity");
  const RoadPageMeshProducer upperOnly(RoadPageKey(request.Address), recipe);
  CHECK(!upperOnly.Produce(request, {}), "Upper-only producers still reject original bodies");
  return Report();
}
