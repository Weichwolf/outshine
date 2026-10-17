#include "TreeGrower.h"
#include "Check.h"
#include <algorithm>
#include <set>
#include <string>

namespace {
using namespace outshine::Generators;
using namespace outshine::Test;

TreeSkeleton Grow(const char *crown, int orders) {
  const std::string text =
      std::string(R"({"name":"narrow crown","height_m":10,"spread_m":2,"dbh_cm":0,
        "crown":")") +
      crown + R"(","bole_frac":0,"base_radius":0.002,"step_len":1,
        "trunk_steps":10,"taper":1,"min_radius":0.000001,"branch_angle":90,
        "branch_angle_var":0,"order_len":1,"order_radius":0.5,"wander":0,
        "leader_bias":0,"branch_up_bias":0,"terminal_fork":0,"whorl_count":1,
        "whorl_spacing":1,"foliate":0,"max_order":)" +
      std::to_string(orders) + "}";
  TreeSpecies species;
  CHECK(species.Parse(text.data(), text.size()), "the controlled horizontal-branch recipe parses");
  TreeGrower grower;
  TreeSkeleton plant;
  grower.Grow(species, plant);
  return plant;
}

std::set<int> BranchLevels(const TreeSkeleton &plant) {
  std::set<int> levels;
  for (const auto &shoot : plant.Shoots) {
    if (shoot.Parent == 0 && shoot.Count > 1) { levels.insert(shoot.ParentNode); }
  }
  return levels;
}
}

int main() {
  const auto free = Grow("free", 1);
  const auto conical = Grow("conical", 1);
  const auto columnar = Grow("columnar", 1);
  const auto bare = Grow("conical", 0);
  const auto expected = BranchLevels(free);
  CHECK(expected.size() == 10, "the free control grows one branch at each declared trunk step");
  CHECK(BranchLevels(bare).empty(), "disabling branching still produces only the trunk");
  CHECK(BranchLevels(conical) == expected,
        "positive conical crown space retains every declared branch level");
  CHECK(BranchLevels(columnar) == expected,
        "narrow curved crown space also retains every declared branch level");
  const auto longest = [](const TreeSkeleton &plant) {
    float reach = 0;
    for (const auto &shoot : plant.Shoots) {
      if (shoot.Parent == 0) { reach = std::max(reach, shoot.Reach); }
    }
    return reach;
  };
  CHECK(longest(conical) < longest(free) && longest(columnar) < longest(free),
        "retaining branches still clips their length to the requested crown");
  CHECK(BranchLevels(Grow("conical", 1)) == expected,
        "identical inputs repeat all retained branch attachments");
  return Report();
}
