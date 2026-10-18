#include "PlantCrownPlan.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace {
outshine::Generators::PlantGrowthPlan Skeleton() {
  return {.PositionsM = {{0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {1, 0, 2}, {0, 0, 3}},
          .Parents = {-1, 0, 1, 2, 2},
          .Unreached = {3},
          .Stats = {}};
}
}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr double kLengthM = 0.35;
  constexpr double kBendRad = 0.4;
  constexpr double kPetioleM = 0.004;
  constexpr size_t kIndexBudget = 60;
  const PlantCrownRecipe recipe{.Seed = 682,
                                .LengthM = kLengthM,
                                .BendRad = kBendRad,
                                .PetioleM = kPetioleM,
                                .FoliageDepthM = 1};
  auto plan = PlantCrownPlan::Create(Skeleton(), {7, 2, 5, 7, 2}, recipe, kIndexBudget);
  CHECK(plan, "valid rooted skeleton accepts a bounded foliage index");
  if (!plan) { return Report(); }
  const auto &summary = plan->Summarize();
  CHECK(summary.Nodes == 5 && summary.Tips == 2 && summary.Shoots == 3 && summary.Leaves == 5 &&
            summary.Variants[7] == 2 && summary.Variants[2] == 2 && summary.Variants[5] == 1 &&
            plan->Skeleton().Unreached == std::vector<uint32_t>{3},
        "summary and original unused growth sources survive without expanding leaf poses");
  CHECK(plan->Hosts()[0] == 2 && plan->Hosts()[1] == 3 && plan->Hosts()[2] == 4,
        "tip-distance band selects ascending original node IDs, excluding the root");
  const auto branch = plan->Leaves({.Node = 2, .Maximum = 2});
  const auto tip = plan->Leaves({.Node = 4, .Maximum = 1});
  const auto repeated = plan->Leaves({.Node = 2, .Maximum = 2});
  CHECK(branch && branch->size() == 2 && (*branch)[0].Id == 0 && (*branch)[1].Id == 1 &&
            (*branch)[0].Variant == 7 && (*branch)[1].Variant == 2 && (*branch)[0].OwnerNode == 2 &&
            tip && tip->size() == 1 && tip->front().Id == 4 && tip->front().Variant == 2,
        "quotient/remainder allocation preserves full source leaf IDs and variant rows");
  CHECK(branch && repeated && branch->front().Pose.OriginM == repeated->front().Pose.OriginM &&
            branch->front().Pose.Frame == repeated->front().Pose.Frame &&
            !plan->Leaves({.Node = 2, .Maximum = 1}) &&
            !plan->MeasureLeaves({.Node = 0, .Maximum = 99}),
        "request order changes neither poses nor identity; insufficient and unknown demands fail");
  auto bare = PlantCrownPlan::Create(Skeleton(), {}, recipe, kIndexBudget);
  const auto empty = bare->Leaves({.Node = 3, .Maximum = 0});
  CHECK(empty && empty->empty(),
        "a valid empty leaf inventory invents neither leaves nor variants");
  PlantGrowthPlan deep;
  constexpr size_t kNodes = 70;
  for (size_t node = 0; node < kNodes; ++node) {
    deep.PositionsM.push_back({0, 0, static_cast<double>(node)});
    deep.Parents.push_back(static_cast<int32_t>(node) - 1);
  }
  const auto longPlan = PlantCrownPlan::Create(std::move(deep), {1}, recipe, kNodes * 12);
  CHECK(longPlan && longPlan->Hosts().back() == kNodes - 1 && longPlan->Summarize().Shoots == 2,
        "deep spatial branches retain their actual parent graph beyond recursive path limits");
  return Report();
}
