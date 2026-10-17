#include "PlantCrownPlan.h"
#include "PlantFrame.h"
#include "PlantGrowth.h"
#include "PlantRandom.h"
#include "PlantShoot.h"
#include "PlantSkeleton.h"
#include "math/Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numbers>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr double kMinimumShootM = 0.01, kMaximumShootM = 2, kMaximumPetioleM = 0.1,
                 kMaximumFoliageDepthM = 5;
constexpr size_t kMaximumLeaves = 100000;
constexpr Vec3 kUp{{0, 0, 1}};

}

bool PlantCrownPlan::ValidRecipe(PlantCrownRecipe recipe) {
  const std::array values{recipe.LengthM,
                          recipe.BendRad,
                          recipe.PetioleM,
                          recipe.FoliageDepthM,
                          recipe.LeafAngleRad,
                          recipe.NormalAlignment};
  return std::ranges::all_of(values, [](double value) { return std::isfinite(value); }) &&
         recipe.LengthM >= kMinimumShootM && recipe.LengthM <= kMaximumShootM &&
         std::abs(recipe.BendRad) <= 1 && recipe.PetioleM >= 0 &&
         recipe.PetioleM <= kMaximumPetioleM && recipe.FoliageDepthM >= 0 &&
         recipe.FoliageDepthM <= kMaximumFoliageDepthM && recipe.LeafAngleRad > 0 &&
         recipe.LeafAngleRad < std::numbers::pi && recipe.NormalAlignment >= 0 &&
         recipe.NormalAlignment < 1;
}

void PlantCrownPlan::IndexHosts() {
  std::vector<double> distance(Growth_.Parents.size(), 0);
  for (size_t child = 1; child < Growth_.Parents.size(); ++child) {
    distance[static_cast<size_t>(Growth_.Parents[child])] = std::numeric_limits<double>::infinity();
  }
  Summary_.Tips = static_cast<size_t>(std::ranges::count(distance, 0));
  for (size_t child = Growth_.Parents.size(); child-- > 1;) {
    const auto parent = static_cast<size_t>(Growth_.Parents[child]);
    const double length =
        Length(Vec3{Growth_.PositionsM[child]} - Vec3{Growth_.PositionsM[parent]});
    distance[parent] = std::min(distance[parent], distance[child] + length);
  }
  Hosts_.reserve(Growth_.Parents.size());
  for (size_t node = 1; node < distance.size(); ++node) {
    if (distance[node] <= Recipe_.FoliageDepthM) { Hosts_.push_back(static_cast<uint32_t>(node)); }
  }
  Summary_.Shoots = Hosts_.size();
}

std::expected<PlantCrownPlan, std::string_view>
PlantCrownPlan::Create(PlantGrowthPlan growth,
                       std::vector<uint8_t> variants,
                       PlantCrownRecipe recipe,
                       size_t maximumIndexBuildBytes) {
  constexpr size_t kBytesPerNode = sizeof(double) + sizeof(uint32_t);
  if (!ValidRecipe(recipe) || variants.size() > kMaximumLeaves ||
      growth.Parents.size() > maximumIndexBuildBytes / kBytesPerNode ||
      !std::ranges::all_of(variants, [](uint8_t variant) { return variant < 64; }) ||
      !ValidPlantSkeleton(growth.PositionsM, growth.Parents)) {
    return std::unexpected(
        "crown requires a valid bounded skeleton, recipe, variants and index budget");
  }
  PlantCrownPlan result;
  result.Growth_ = std::move(growth);
  result.Variants_ = std::move(variants);
  result.Recipe_ = recipe;
  result.Summary_.Nodes = result.Growth_.Parents.size();
  result.Summary_.Leaves = result.Variants_.size();
  result.Summary_.IndexBuildBytes = result.Growth_.Parents.size() * kBytesPerNode;
  for (const uint8_t variant : result.Variants_) { ++result.Summary_.Variants[variant]; }
  result.IndexHosts();
  result.Summary_.IndexResidentBytes = result.Hosts_.capacity() * sizeof(uint32_t);
  return result;
}

size_t PlantCrownPlan::FirstLeaf(size_t host) const noexcept {
  return host * (Variants_.size() / Hosts_.size()) +
         std::min(host, Variants_.size() % Hosts_.size());
}

size_t PlantCrownPlan::LeafCount(size_t host) const noexcept {
  return Variants_.size() / Hosts_.size() + (host < Variants_.size() % Hosts_.size() ? 1 : 0);
}

std::expected<size_t, std::string_view>
PlantCrownPlan::HostIndex(LeafDemand demand) const noexcept {
  const auto found = std::ranges::lower_bound(Hosts_, demand.Node);
  if (found == Hosts_.end() || *found != demand.Node) {
    return std::unexpected("crown foliage host is absent");
  }
  const auto index = static_cast<size_t>(found - Hosts_.begin());
  if (LeafCount(index) > demand.Maximum) {
    return std::unexpected("crown foliage group exceeds leaf demand");
  }
  return index;
}

std::expected<size_t, std::string_view>
PlantCrownPlan::MeasureLeaves(LeafDemand demand) const noexcept {
  const auto index = HostIndex(demand);
  if (!index) { return std::unexpected(index.error()); }
  return LeafCount(*index);
}

std::expected<std::vector<PlantCrownLeaf>, std::string_view>
PlantCrownPlan::Leaves(LeafDemand demand) const {
  const auto index = HostIndex(demand);
  if (!index) { return std::unexpected(index.error()); }
  const size_t count = LeafCount(*index);
  if (count == 0) { return std::vector<PlantCrownLeaf>{}; }
  const size_t first = FirstLeaf(*index);
  const uint32_t node = demand.Node;
  const Vec3 origin{Growth_.PositionsM[node]};
  const auto frame = PlantNormalFrame(
      origin - Vec3{Growth_.PositionsM[static_cast<size_t>(Growth_.Parents[node])]}, kUp);
  if (!frame) { return std::unexpected("crown host frame is not representable"); }
  PlantRandom random(Recipe_.Seed ^ node);
  const double phase = random.Uniform(-std::numbers::pi, std::numbers::pi);
  constexpr double kLeafLast = 0.95;
  constexpr double kLeafSpan = 0.9;
  constexpr double kHalf = 0.5;
  const double internode =
      Recipe_.LengthM * kLeafSpan / std::max(kHalf, static_cast<double>(count) - kHalf);
  constexpr double kLeafFirst = 0.05;
  const auto recipeCount =
      static_cast<size_t>(std::floor((kLeafLast - kLeafFirst) * Recipe_.LengthM / internode)) + 1;
  if (recipeCount != count) {
    return std::unexpected("crown shoot count recipe is not representable");
  }
  const PlantShoot shoot{.LengthM = Recipe_.LengthM,
                         .BendRad = Recipe_.BendRad,
                         .PetioleM = Recipe_.PetioleM,
                         .LeafAngleRad = Recipe_.LeafAngleRad,
                         .NormalAlignment = Recipe_.NormalAlignment};
  const double increment = std::numbers::pi * (3 - std::sqrt(5));
  std::vector<PlantCrownLeaf> leaves;
  leaves.reserve(count);
  for (size_t row = 0; row < count; ++row) {
    const double parameter = kLeafLast - static_cast<double>(row) * internode / Recipe_.LengthM;
    auto pose = shoot.LeafAt(
        {.Parameter = parameter, .AzimuthRad = phase + static_cast<double>(row) * increment});
    if (!pose) { return std::unexpected("crown leaf frame is not representable"); }
    pose->NodeM = (PlantRotate(*frame, Vec3{pose->NodeM}) + origin).Axis;
    pose->OriginM = (PlantRotate(*frame, Vec3{pose->OriginM}) + origin).Axis;
    pose->Frame = PlantComposeFrames(*frame, pose->Frame);
    leaves.push_back(
        {.Id = first + row, .OwnerNode = node, .Variant = Variants_[first + row], .Pose = *pose});
  }
  return leaves;
}
}
