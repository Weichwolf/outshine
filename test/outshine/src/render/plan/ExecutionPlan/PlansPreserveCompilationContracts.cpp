#include "ExecutionPlan.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <limits>
#include <print>

namespace {
constexpr int kUnknownEnumValue = 255;

void Snapshot(size_t index, const outshine::Render::ExecutionPlan &plan) {
  using namespace outshine::Render;
  std::print("PLAN {} {} 0x{:a} {} {} {}",
             index,
             plan.Digest(),
             static_cast<double>(plan.Exposure()),
             static_cast<int>(plan.Display()),
             static_cast<int>(plan.Precision()),
             plan.SettleFrames());
  for (size_t at = 0; at < kResourceCount; ++at) {
    const auto resource = static_cast<Resource>(at);
    std::print(" R{}:{},{},{},{}",
               at,
               static_cast<int>(plan.Holds(resource)),
               static_cast<int>(plan.Stored(resource)),
               static_cast<int>(plan.Bound(resource)),
               static_cast<int>(plan.Format(resource)));
  }
  for (const auto stage : plan.Order()) {
    std::print(" S{}:{}", static_cast<int>(stage), static_cast<int>(plan.Fused(stage)));
  }
  for (const auto &pass : plan.Passes()) {
    std::print(" P{}:{},{},{},{}",
               pass.Name,
               static_cast<int>(pass.Kind),
               pass.First,
               pass.Count,
               static_cast<int>(pass.Depth));
    for (const auto target : pass.Targets) { std::print(" T{}", static_cast<int>(target)); }
    for (const auto buffer : pass.Buffers) { std::print(" B{}", static_cast<int>(buffer)); }
  }
  std::println("");
}
}

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  const std::array specs{PlanSpec{.Outputs = {Resource::SceneHdr},
                                  .Content = {Stage::OpaqueGeometry, Stage::Sky},
                                  .Exposure = {},
                                  .Display = {},
                                  .Precision = {}},
                         PlanSpec{.Outputs = {Resource::FrameTex},
                                  .Content = {Stage::OpaqueGeometry, Stage::Sky},
                                  .Exposure = {},
                                  .Display = {},
                                  .Precision = {}},
                         PlanSpec{.Outputs = {Resource::FrameTex},
                                  .Content = {Stage::OpaqueGeometry,
                                              Stage::Sky,
                                              Stage::TemporalResolve,
                                              Stage::AutoExposure},
                                  .Exposure = {},
                                  .Display = {},
                                  .Precision = {}},
                         PlanSpec{.Outputs = {Resource::FrameTex},
                                  .Content = {Stage::OpaqueGeometry, Stage::Sky},
                                  .Exposure = Declared<float>(2),
                                  .Display = Declared<Transfer>(Transfer::Linear),
                                  .Precision = Declared<ScenePrecision>(ScenePrecision::Float)}};
  for (size_t index = 0; index < specs.size(); ++index) {
    const auto result = ExecutionPlan::Compile(specs[index]);
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
    CHECK(!ExecutionPlan::Compile(invalid), "invalid exposure rejected");
  }
  auto invalid = specs[1];
  invalid.Display = Declared<Transfer>(static_cast<Transfer>(kUnknownEnumValue));
  CHECK(!ExecutionPlan::Compile(invalid), "unknown display transfer rejected");
  invalid = specs[1];
  invalid.Precision = Declared<ScenePrecision>(static_cast<ScenePrecision>(kUnknownEnumValue));
  CHECK(!ExecutionPlan::Compile(invalid), "unknown precision rejected");
  for (const auto id :
       {-1, static_cast<int>(kResourceCount), static_cast<int>(kResourceCount) + 1}) {
    invalid = specs[1];
    invalid.Outputs = {static_cast<Resource>(id)};
    CHECK(!ExecutionPlan::Compile(invalid), "unknown output rejected before dependency indexing");
  }
  for (const auto id : {-1, static_cast<int>(kStageCount), static_cast<int>(kStageCount) + 1}) {
    invalid = specs[1];
    invalid.Content = {static_cast<Stage>(id)};
    CHECK(!ExecutionPlan::Compile(invalid), "unknown stage rejected before dependency indexing");
  }
  auto zero = specs[1];
  zero.Exposure = Declared<float>(0);
  const auto dark = ExecutionPlan::Compile(zero);
  CHECK(dark && (*dark)->Exposure() == 0, "zero exposure is a valid scale");
  return Report();
}
