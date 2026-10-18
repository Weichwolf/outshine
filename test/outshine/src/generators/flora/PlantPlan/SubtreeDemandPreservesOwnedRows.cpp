#include "Check.h"
#include "PlantPlan.h"

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace {
using outshine::Generators::PlantAxis;
using outshine::Generators::PlantLeaf;
using outshine::Generators::PlantPlan;
constexpr uint64_t kRoot = 10;
constexpr uint64_t kLeft = 20;
constexpr uint64_t kRight = 30;
constexpr uint64_t kTwig = 40;
constexpr uint64_t kFirstLeaf = 100;
constexpr uint64_t kSecondLeaf = 101;
constexpr uint64_t kThirdLeaf = 102;
constexpr uint64_t kMissing = 999;
constexpr double kBaseRadiusM = 0.02;
constexpr double kTipRadiusM = 0.01;
constexpr std::array<double, 9> kFrame{1, 0, 0, 0, 1, 0, 0, 0, 1};

std::vector<PlantAxis> Axes() {
  PlantAxis root;
  root.Id = kRoot;
  root.Frame = kFrame;
  root.LengthM = 1;
  root.RadiiM = {kBaseRadiusM, kTipRadiusM};
  PlantAxis left = root;
  PlantAxis right = root;
  PlantAxis twig = root;
  left.Id = kLeft;
  left.Parent = right.Parent = kRoot;
  left.Path[0] = 0;
  right.Id = kRight;
  right.Path[0] = 1;
  twig.Id = kTwig;
  twig.Parent = kLeft;
  twig.Path[0] = twig.Path[1] = 0;
  return {root, left, right, twig};
}

std::vector<PlantLeaf> Leaves() {
  PlantLeaf first;
  first.Id = kFirstLeaf;
  first.Axis = kLeft;
  first.Frame = kFrame;
  PlantLeaf second = first;
  PlantLeaf third = first;
  second.Id = kSecondLeaf;
  second.Axis = kRight;
  second.Variant = 1;
  third.Id = kThirdLeaf;
  third.Axis = kTwig;
  third.Variant = 1;
  return {first, second, third};
}
}

int main() {
  using namespace outshine::Test;
  auto plan = PlantPlan::Create(Axes(), Leaves(), 2);
  CHECK(plan.has_value(), "canonical breadth-first plant rows load");
  if (!plan) { return Report(); }
  const auto root = plan->Summarize(kRoot);
  const auto branch = plan->Summarize(kLeft);
  CHECK(root && root->Axes == 4 && root->Leaves == 3 && root->Variants[0] == 1 &&
            root->Variants[1] == 2,
        "root summary accounts for all descendants before detail");
  CHECK(branch && branch->Axes == 2 && branch->Leaves == 2 && branch->Variants[0] == 1 &&
            branch->Variants[1] == 1,
        "partial summary excludes the neighbouring branch");
  CHECK(branch && branch->AxisRows.size() == 2 && branch->AxisRows[0] == 1 &&
            branch->AxisRows[1] == 3,
        "prepared subtree rows skip the unrelated breadth-first sibling without a plan scan");
  CHECK(!plan->Leaves({.Axis = kLeft, .Maximum = 0}),
        "an insufficient detail budget refuses the whole leaf group");
  const auto leaves = plan->Leaves({.Axis = kLeft, .Maximum = 1});
  const auto again = plan->Leaves({.Axis = kLeft, .Maximum = 1});
  CHECK(leaves && again && leaves->size() == 1 && leaves->front().Id == kFirstLeaf &&
            leaves->data() == again->data(),
        "detail retains ownership, stable ID and shared immutable storage");
  CHECK(plan->Leaves({.Axis = kRoot, .Maximum = 0}) &&
            plan->Leaves({.Axis = kRoot, .Maximum = 0})->empty(),
        "bare axes have no invented leaves");
  CHECK(!plan->Axis(kMissing) && !plan->Summarize(kMissing) &&
            !plan->Leaves({.Axis = kMissing, .Maximum = 1}),
        "unknown IDs do not alias the root");
  auto axes = Axes();
  axes[3].Parent = kTwig;
  CHECK(!PlantPlan::Create(axes, Leaves(), 2), "self-parented axes are rejected");
  axes = Axes();
  axes[2].Path = axes[1].Path;
  CHECK(!PlantPlan::Create(axes, Leaves(), 2), "duplicate paths are rejected");
  axes = Axes();
  axes[1].Frame[0] = -1;
  CHECK(!PlantPlan::Create(axes, Leaves(), 2), "left-handed frames are rejected");
  auto invalid = Leaves();
  invalid[0].Id = invalid[1].Id;
  CHECK(!PlantPlan::Create(Axes(), invalid, 2), "duplicate leaf identities are rejected");
  invalid = Leaves();
  invalid[0].Axis = kMissing;
  CHECK(!PlantPlan::Create(Axes(), invalid, 2), "unknown owners are rejected");
  invalid = Leaves();
  invalid[0].Frame[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!PlantPlan::Create(Axes(), invalid, 2), "nonfinite frames are rejected");
  invalid = Leaves();
  std::swap(invalid[0], invalid[1]);
  CHECK(!PlantPlan::Create(Axes(), invalid, 2), "noncanonical owner order is rejected");
  return Report();
}
