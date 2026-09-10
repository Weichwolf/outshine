#include "Compiled.h"
#include "Check.h"
#include <array>
#include <cstdio>
#include <limits>

namespace {
void Snapshot(size_t index, const outshine::Render::Compiled &plan) {
  using namespace outshine::Render;
  std::printf("PLAN %zu %s %a %d %d %d",
              index,
              plan.Digest().c_str(),
              static_cast<double>(plan.Exposure()),
              static_cast<int>(plan.Display()),
              static_cast<int>(plan.Precision()),
              plan.SettleFrames());
  for (size_t at = 0; at < kResourceCount; ++at) {
    const auto resource = static_cast<Resource>(at);
    std::printf(" R%zu:%d,%d,%d,%d",
                at,
                plan.Holds(resource),
                plan.Stored(resource),
                static_cast<int>(plan.Bound(resource)),
                static_cast<int>(plan.Format(resource)));
  }
  for (const auto stage : plan.Order()) {
    std::printf(" S%d:%d", static_cast<int>(stage), plan.Fused(stage));
  }
  for (const auto &pass : plan.Passes()) {
    std::printf(" P%s:%d,%zu,%zu,%d",
                pass.Name.c_str(),
                static_cast<int>(pass.Kind),
                pass.First,
                pass.Count,
                static_cast<int>(pass.Depth));
    for (const auto target : pass.Targets) { std::printf(" T%d", static_cast<int>(target)); }
    for (const auto buffer : pass.Buffers) { std::printf(" B%d", static_cast<int>(buffer)); }
  }
  std::printf("\n");
}
}

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  const std::array specs{
      PlanSpec{.Outputs = {Resource::SceneHdr}, .Content = {Stage::Subjects, Stage::Sky}},
      PlanSpec{.Outputs = {Resource::FrameTex}, .Content = {Stage::Subjects, Stage::Sky}},
      PlanSpec{
          .Outputs = {Resource::FrameTex},
          .Content = {Stage::Subjects, Stage::Sky, Stage::TemporalResolve, Stage::AutoExposure}},
      PlanSpec{.Outputs = {Resource::FrameTex},
               .Content = {Stage::Subjects, Stage::Sky},
               .Exposure = Declared<float>(2),
               .Display = Declared<Transfer>(Transfer::Linear),
               .Precision = Declared<ScenePrecision>(ScenePrecision::Float)}};
  for (size_t index = 0; index < specs.size(); ++index) {
    const auto result = Compiled::Compile(specs[index]);
    CHECK(result.has_value(), "representative plan compiles");
    if (!result) { continue; }
    const auto &plan = **result;
    Snapshot(index, plan);
    size_t next = 0;
    for (const auto &pass : plan.Passes()) {
      CHECK(pass.First == next && pass.Count > 0, "passes partition ordered stages without gaps");
      next += pass.Count;
    }
    CHECK(next == plan.Order().size(), "every ordered stage belongs to a pass");
    for (const auto output : specs[index].Outputs) {
      CHECK(plan.Stored(plan.Bound(output)), "requested outputs survive the pass");
    }
    if (index == 2) {
      CHECK(plan.Fused(Stage::Tonemap), "temporal resolve fuses with display transfer");
    }
    if (index == 3) {
      CHECK(plan.Format(Resource::SceneHdr) == TexelFormat::Rgba32Float && plan.Exposure() == 2,
            "explicit radiance precision and exposure retained");
    }
  }
  for (const float exposure :
       {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    auto invalid = specs[1];
    invalid.Exposure = Declared<float>(exposure);
    CHECK(!Compiled::Compile(invalid), "invalid exposure rejected");
  }
  auto invalid = specs[1];
  invalid.Display = Declared<Transfer>(static_cast<Transfer>(255));
  CHECK(!Compiled::Compile(invalid), "unknown display transfer rejected");
  invalid = specs[1];
  invalid.Precision = Declared<ScenePrecision>(static_cast<ScenePrecision>(255));
  CHECK(!Compiled::Compile(invalid), "unknown precision rejected");
  for (const auto id :
       {-1, static_cast<int>(kResourceCount), static_cast<int>(kResourceCount) + 1}) {
    invalid = specs[1];
    invalid.Outputs = {static_cast<Resource>(id)};
    CHECK(!Compiled::Compile(invalid), "unknown output rejected before dependency indexing");
  }
  for (const auto id : {-1, static_cast<int>(kStageCount), static_cast<int>(kStageCount) + 1}) {
    invalid = specs[1];
    invalid.Content = {static_cast<Stage>(id)};
    CHECK(!Compiled::Compile(invalid), "unknown stage rejected before dependency indexing");
  }
  auto zero = specs[1];
  zero.Exposure = Declared<float>(0);
  const auto dark = Compiled::Compile(zero);
  CHECK(dark && (*dark)->Exposure() == 0, "zero exposure is a valid scale");
  return Report();
}
