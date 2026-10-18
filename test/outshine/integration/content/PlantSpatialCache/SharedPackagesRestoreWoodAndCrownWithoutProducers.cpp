#include "content/AssetGeneration.h"
#include "PlantCrownAsset.h"
#include "PlantCrownPlan.h"
#include "PlantLimbAsset.h"
#include "PlantLimbPlan.h"
#include "PlantLimbSurface.h"
#include "Sha256.h"
#include "Check.h"
#include "Shell.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kBudget = 4096;

std::string Request(std::string_view kind) {
  const std::string demand = "plant-spatial/v1/fork/seed682/taper1/" + std::string(kind);
  return Sha256Hex(demand.data(), demand.size());
}

std::string Quote(std::string_view text) {
  std::string result = "'";
  for (const char one : text) { result += one == '\'' ? "'\\''" : std::string(1, one); }
  return result + '\'';
}

std::expected<GeneratedAssetPackage, std::string> Generate(size_t budget) {
  PlantGrowthPlan growth{.PositionsM = {{0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {1, 0, 2}},
                         .Parents = {-1, 0, 1, 1},
                         .Unreached = {},
                         .Stats = {}};
  const PlantCrownRecipe recipe{.Seed = 682, .LengthM = 0.35, .FoliageDepthM = 1};
  auto crown = PlantCrownPlan::Create(std::move(growth), {7, 2, 5, 7, 2}, recipe, 48);
  if (!crown) { return std::unexpected(std::string(crown.error())); }
  constexpr std::array mainAnchors{
      PlantLimbAnchor{.Node = 0, .PositionM = {{0, 0, 0}}, .RadiusM = 0.1},
      PlantLimbAnchor{.Node = 1, .PositionM = {{0, 0, 1}}, .RadiusM = 0.075},
      PlantLimbAnchor{.Node = 2, .PositionM = {{0, 0, 2}}, .RadiusM = 0.05}};
  constexpr std::array sideAnchors{
      PlantLimbAnchor{.Node = 1, .PositionM = {{0, 0, 1}}, .RadiusM = 0.025},
      PlantLimbAnchor{.Node = 3, .PositionM = {{1, 0, 2}}, .RadiusM = 0.01}};
  auto main = PlantLimbPlan::Create(mainAnchors);
  auto side = PlantLimbPlan::Create(sideAnchors);
  if (!main || !side) { return std::unexpected("source limb preparation failed"); }
  std::vector<PlantLimbPlan> limbs;
  limbs.push_back(std::move(*main));
  limbs.push_back(std::move(*side));
  auto foliage = EncodePlantCrownPlan(*crown, budget);
  auto wood = EncodePlantLimbs(limbs, budget);
  if (!foliage || !wood || foliage->size() + wood->size() > budget) {
    return std::unexpected("prepared spatial package exceeds budget");
  }
  const std::string crownKey = Sha256Hex(foliage->data(), foliage->size());
  const std::string woodKey = Sha256Hex(wood->data(), wood->size());
  GeneratedAssetPackage package{.Records = {{.Key = crownKey,
                                             .Kind = "plant-crown-plan",
                                             .Bounds = {.Min = {{-1, -1, -1}}, .Max = {{2, 1, 3}}},
                                             .Package = {},
                                             .ByteCount = foliage->size(),
                                             .Parent = {},
                                             .RequestKey = Request("crown")},
                                            {.Key = woodKey,
                                             .Kind = "plant-limb-plan",
                                             .Bounds = {.Min = {{-1, -1, -1}}, .Max = {{2, 1, 3}}},
                                             .Package = {},
                                             .OffsetBytes = foliage->size(),
                                             .ByteCount = wood->size(),
                                             .Parent = crownKey,
                                             .RequestKey = Request("wood")}},
                                .Bytes = std::move(*foliage)};
  package.Bytes.insert(package.Bytes.end(), wood->begin(), wood->end());
  return package;
}

int Hit(const std::string &path) {
  auto cache = AssetCache::Open(path);
  if (!cache) { return 1; }
  const auto crown = ResolveAssetRequest(**cache, Request("crown"), kBudget, {});
  const auto wood = ResolveAssetRequest(**cache, Request("wood"), kBudget, {});
  if (!crown || !wood || crown->Record().Package != wood->Record().Package ||
      wood->Record().Parent != crown->Record().Key ||
      wood->Record().OffsetBytes != crown->Bytes().size()) {
    return 2;
  }
  const auto foliage = DecodePlantCrownPlan(crown->Bytes(), kBudget);
  const auto limbs = DecodePlantLimbs(wood->Bytes(), kBudget);
  if (!foliage || !limbs || limbs->size() != 2 || foliage->Summarize().IndexBuildBytes != 0) {
    return 3;
  }
  for (const auto &limb : *limbs) {
    for (size_t at = 0; at < limb.Nodes().size(); ++at) {
      const auto sample = limb.Sample(static_cast<double>(at) / (limb.Nodes().size() - 1));
      const auto node = limb.Nodes()[at];
      if (!sample || sample->CentreM.Axis != foliage->Skeleton().PositionsM[node]) { return 4; }
    }
    const auto surface = BuildPlantLimbSurface(
        limb, {.Along = 2, .Sides = 8, .MaximumVertices = 65536, .MaximumTriangles = 262144});
    if (!surface) { return 5; }
  }
  const auto leaves = foliage->Leaves({.Node = 2, .Maximum = 2});
  return leaves && leaves->size() == 2 && leaves->front().Id == 2 &&
                 leaves->front().OwnerNode == 2 && leaves->front().Variant == 5
             ? 0
             : 6;
}
}

int main(int argc, char *const *argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--hit") { return Hit(argv[2]); }
  const std::string path = PlantedPath("native-spatial-plant.sqlite");
  std::error_code error;
  std::filesystem::remove(path, error);
  size_t builds = 0;
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache, "common cache opens before either source product is available");
    if (!cache) { return Report(); }
    const AssetFactory generate = [&](size_t budget) {
      ++builds;
      return Generate(budget);
    };
    const auto crown = ResolveAssetRequest(**cache, Request("crown"), kBudget, generate);
    const auto wood = ResolveAssetRequest(**cache, Request("wood"), kBudget, {});
    CHECK(crown && wood && builds == 1,
          "one miss publishes both prepared products in one shared package");
  }
  std::string output;
  const int child = Run(Quote(argv[0]) + " --hit " + Quote(path), output);
  CHECK(child == 0 && builds == 1,
        "fresh process restores wood and crown with no registered producer or source inputs");
  std::filesystem::remove(path, error);
  return Report();
}
