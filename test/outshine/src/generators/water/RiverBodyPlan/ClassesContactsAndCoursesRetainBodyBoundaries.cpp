#include "RiverBodyPlan.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;

uint32_t Input(WaterAsset::Data &data, uint32_t source, const char *layer, const char *kind) {
  const auto index = static_cast<uint32_t>(data.Inputs.size());
  data.Inputs.push_back({.Source = source,
                         .Feature = index,
                         .FeatureId = {},
                         .Layer = layer,
                         .Kind = kind,
                         .Crossing = {},
                         .Intermittent = {}});
  return index;
}

void Ring(WaterAsset::Data &data, double x, double y, double width, double height) {
  constexpr double step = .0001;
  data.Rings.push_back(
      {.FirstPoint = static_cast<uint32_t>(data.Points.size() / 2), .PointCount = 4});
  for (const auto p : std::array<std::array<double, 2>, 4>{
           {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}}}) {
    data.Points.insert(data.Points.end(), {p[1] * step, p[0] * step});
  }
}

void Surface(WaterAsset::Data &data, double x, double y, double width, double height) {
  data.Surfaces.push_back({.FirstRing = static_cast<uint32_t>(data.Rings.size()),
                           .RingCount = 1,
                           .LevelM = 100,
                           .Input = Input(data, 0, "water_polygons", "river")});
  Ring(data, x, y, width, height);
}

void Course(WaterAsset::Data &data, double x, double y, double toX, double toY) {
  constexpr double step = .0001;
  data.Courses.push_back({.FirstPoint = static_cast<uint32_t>(data.Points.size() / 2),
                          .PointCount = 2,
                          .FirstLevel = static_cast<uint32_t>(data.Levels.size()),
                          .Input = Input(data, 0, "water_lines", "river")});
  data.Points.insert(data.Points.end(), {y * step, x * step, toY * step, toX * step});
  data.Levels.insert(data.Levels.end(), {100, 99});
}

WaterAsset Source() {
  WaterAsset::Data data;
  data.Sources.push_back(
      {.Identity = {.Kind = Data::DataKind::VectorMap, .SourceId = "fixture", .Revision = "r1"},
       .InputDigest = {},
       .FeatureCount = 32});
  data.Sources.push_back(
      {.Identity = {.Kind = Data::DataKind::VectorMap, .SourceId = "fixture", .Revision = "r2"},
       .InputDigest = {},
       .FeatureCount = 32});
  Surface(data, 0, 0, 1, 1);
  Surface(data, 1, 0, 1, 1);
  Surface(data, 2, 1, 1, 1);
  Surface(data, 5, 0, 4, 4);
  Ring(data, 6, 1, 2, 2);
  data.Surfaces.back().RingCount = 2;
  Surface(data, 6.5, 1.5, 1, 1);
  Surface(data, 0, 0, 1, 1);
  data.Inputs.back().Kind = "lake";
  Surface(data, 0, 0, 1, 1);
  data.Inputs.back().Bridge = true;
  Surface(data, 0, 0, 1, 1);
  data.Inputs.back().Source = 1;
  Course(data, -.5, .5, 2.5, .5);
  Course(data, 2, 1, 2.5, .5);
  Course(data, -.5, .5, 2.5, .5);
  data.Inputs.back().Kind = "stream";
  Course(data, -.5, .5, 2.5, .5);
  data.Inputs.back().Bridge = true;
  Course(data, -.5, .5, 2.5, .5);
  data.Inputs.back().Source = 1;
  Course(data, 6.6, 2, 7.4, 2);
  return WaterAsset(std::move(data));
}

std::vector<uint32_t> Sources(const RiverBody &body) {
  auto sources = body.Surface.FaceSources;
  std::ranges::sort(sources);
  const auto duplicate = std::ranges::unique(sources);
  sources.erase(duplicate.begin(), duplicate.end());
  return sources;
}
}

int main() {
  const auto water = Source();
  const auto frame = TangentFrame::At({});
  const auto plan = PlanRiverBodies(water, frame, WaterGroundPolicy::AssumeUnlabelledGround);
  CHECK(plan && plan->Rejections.empty() && plan->Bodies.size() == 5,
        "edge-connected rivers merge while point contacts, hole interiors and revisions separate");
  if (!plan || plan->Bodies.size() != 5) { return outshine::Test::Report(); }
  const std::array<std::vector<uint32_t>, 5> expected{{{0, 1}, {2}, {3}, {4}, {7}}};
  const std::array<std::vector<uint32_t>, 5> courses{{{0}, {}, {}, {5}, {4}}};
  for (size_t at = 0; at < plan->Bodies.size(); ++at) {
    CHECK(Sources(plan->Bodies[at]) == expected[at],
          "body membership retains complete original surface ordinals");
    CHECK(
        plan->Bodies[at].Courses == courses[at],
        "courses require matching provenance/class, resolved crossing and positive line coverage");
    CHECK(plan->Bodies[at].Surface.FaceSourceOffsets.size() ==
              plan->Bodies[at].Surface.Faces.size() + 1,
          "component splitting preserves complete per-face source coverage");
  }
  const auto separate = PlanRiverBodies(water, frame, WaterGroundPolicy::SeparateInputs);
  CHECK(separate && separate->Bodies.size() == 6 && separate->EnvelopePairs == 0,
        "unlabelled ground connection is an explicit policy rather than a source observation");
  std::stop_source canceled;
  canceled.request_stop();
  const auto stopped = PlanRiverBodies(
      water, frame, WaterGroundPolicy::AssumeUnlabelledGround, {}, canceled.get_token());
  CHECK(!stopped && stopped.error() == WaterTopologyError::Canceled,
        "canceled work cannot publish a partial river plan");
  return outshine::Test::Report();
}
