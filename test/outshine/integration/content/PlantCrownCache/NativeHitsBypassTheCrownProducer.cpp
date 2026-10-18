#include "content/AssetGeneration.h"
#include "PlantCrownAsset.h"
#include "PlantCrownPlan.h"
#include "Sha256.h"
#include "Check.h"
#include "Shell.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kBudget = 4096;

std::string Request() {
  constexpr std::string_view kDemand = "plant-crown-plan/v1/small-fork/seed682";
  return Sha256Hex(kDemand.data(), kDemand.size());
}

std::string Quote(std::string_view text) {
  std::string result = "'";
  for (const char one : text) { result += one == '\'' ? "'\\''" : std::string(1, one); }
  return result + '\'';
}

int Hit(const std::string &path) {
  auto cache = AssetCache::Open(path);
  if (!cache) { return 1; }
  const auto hit = ResolveAssetRequest(**cache, Request(), kBudget, {});
  if (!hit) { return 2; }
  const auto plan = DecodePlantCrownPlan(hit->Bytes(), kBudget);
  if (!plan || plan->Summarize().IndexBuildBytes != 0) { return 3; }
  const auto leaves = plan->Leaves({.Node = 2, .Maximum = 2});
  if (!leaves || leaves->size() != 2 || leaves->front().Id != 0 || leaves->front().OwnerNode != 2 ||
      leaves->front().Variant != 7) {
    return 4;
  }
  return 0;
}
}

int main(int argc, char *const *argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--hit") { return Hit(argv[2]); }
  const std::string path = PlantedPath("native-crown-plan.sqlite");
  std::error_code error;
  std::filesystem::remove(path, error);
  size_t builds = 0;
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache, "shared native asset cache opens before any source plan exists");
    if (!cache) { return Report(); }
    const AssetFactory generate =
        [&](size_t budget) -> std::expected<GeneratedAssetPackage, std::string> {
      ++builds;
      PlantGrowthPlan growth{.PositionsM = {{0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {1, 0, 2}, {0, 0, 3}},
                             .Parents = {-1, 0, 1, 2, 2},
                             .Unreached = {},
                             .Stats = {}};
      constexpr double kLengthM = 0.35;
      const PlantCrownRecipe recipe{.Seed = 682, .LengthM = kLengthM, .FoliageDepthM = 1};
      const auto plan = PlantCrownPlan::Create(std::move(growth), {7, 2, 5, 7, 2}, recipe, 60);
      if (!plan) { return std::unexpected(std::string(plan.error())); }
      auto bytes = EncodePlantCrownPlan(*plan, budget);
      if (!bytes) { return std::unexpected(std::string(bytes.error())); }
      const std::string key = Sha256Hex(bytes->data(), bytes->size());
      return GeneratedAssetPackage{
          .Records = {{.Key = key,
                       .Kind = "plant-crown-plan",
                       .Bounds = {.Min = {{-1, -1, -1}}, .Max = {{2, 1, 4}}},
                       .Package = {},
                       .ByteCount = bytes->size(),
                       .Parent = {},
                       .RequestKey = Request()}},
          .Bytes = std::move(*bytes)};
    };
    const auto prepared = ResolveAssetRequest(**cache, Request(), kBudget, generate);
    CHECK(prepared && builds == 1, "only the miss creates and publishes the complete plan/index");
  }
  std::string output;
  const int child = Run(Quote(argv[0]) + " --hit " + Quote(path), output);
  CHECK(child == 0 && builds == 1,
        "fresh process restores the same demand with no producer, index construction or source "
        "input");
  std::filesystem::remove(path, error);
  return Report();
}
