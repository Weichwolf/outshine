#include "PlantRecipe.h"
#include "PlantRecipeJson.h"
#include <array>
#include <string_view>
#include <utility>
#include "PlantPlan.h"
#include "PlantBladeSurface.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <limits>

namespace {
using namespace outshine::Generators;

template <class T> void Write(const T &values) {
  for (const auto value : values) { std::cout << ' ' << value; }
}

bool ReadLevel(PlantLevel &level) {
  unsigned shape = 0;
  unsigned arrangement = 0;
  unsigned leaf = 0;
  unsigned tip = 0;
  double internode = 0;
  double angle = 0;
  if (!(std::cin >> level.Branches >> level.LengthM >> level.BaseRadiusM >> level.TipRadiusM >>
        level.TipScale >> shape >> level.AttachmentJitter >> level.AzimuthVariationRad >>
        level.UpwardBias >> level.First >> level.Last >> level.AngleRad >>
        level.AngleVariationRad >> level.BendRad >> level.BendVariationRad >> leaf >> internode >>
        level.LeafFirst >> level.LeafLast >> level.LeafAngleRad >> level.LeafSkyAlignment >>
        arrangement >> level.WhorlSize >> tip >> angle) ||
      shape > 1 || arrangement > 4 || leaf > 1 || tip > 1) {
    return false;
  }
  level.Shape = static_cast<PlantBranchShape>(shape);
  level.Arrangement = static_cast<PlantArrangement>(arrangement);
  if (leaf != 0) { level.LeafInternodeM = internode; }
  if (tip != 0) { level.TipAngleRad = angle; }
  return true;
}

bool ReadBlade(PlantBlade &blade) {
  return static_cast<bool>(std::cin >> blade.LengthM >> blade.WidthM >> blade.Widest >>
                           blade.Lobes >> blade.LobeDepth >> blade.BaseFill >> blade.BendRad >>
                           blade.Fold >> blade.FoldPower >> blade.TwistRad >> blade.PetioleM);
}

void WriteRows(const PlantPlan &plan, size_t maximumLeaves) {
  for (const auto &axis : plan.Axes()) {
    std::cout << "A " << axis.Id << ' ' << axis.Parent;
    Write(axis.Path);
    Write(axis.OriginM);
    Write(axis.Frame);
    std::cout << ' ' << axis.LengthM << ' ' << axis.Bend << ' ' << axis.ParentParameter;
    Write(axis.RadiiM);
    std::cout << '\n';
    const auto leaves = plan.Leaves({.Axis = axis.Id, .Maximum = maximumLeaves});
    if (!leaves) { return; }
    for (const auto &leaf : *leaves) {
      std::cout << "L " << leaf.Id << ' ' << leaf.Axis << ' '
                << static_cast<unsigned>(leaf.Variant);
      Write(leaf.OriginM);
      Write(leaf.NodeM);
      Write(leaf.Frame);
      std::cout << ' ' << leaf.StemParameter << '\n';
    }
  }
}
}

namespace {
bool ReadDemand(PlantRecipeDemand &demand) {
  for (auto &slot : demand.Path) {
    if (!(std::cin >> slot)) { return false; }
  }
  return static_cast<bool>(std::cin >> demand.MaximumAxes >> demand.MaximumLeaves);
}

bool ReadRecipe(PlantRecipe &recipe) {
  size_t levels = 0;
  size_t blades = 0;
  if (!(std::cin >> recipe.Seed >> levels >> blades) || levels == 0 || levels > 8 || blades > 64) {
    return false;
  }
  recipe.Levels.resize(levels);
  recipe.Blades.resize(blades);
  for (auto &level : recipe.Levels) {
    if (!ReadLevel(level)) { return false; }
  }
  for (auto &blade : recipe.Blades) {
    if (!ReadBlade(blade)) { return false; }
  }
  return true;
}

bool ReadJson(PlantRecipe &recipe) {
  constexpr size_t kMostInputBytes = size_t{64} * 1024;
  std::array<char, kMostInputBytes + 1> buffer{};
  std::cin.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  const auto count = static_cast<size_t>(std::cin.gcount());
  auto parsed = ReadPlantRecipe(std::string_view(buffer.data(), count), kMostInputBytes);
  if (!parsed) {
    std::cerr << parsed.error();
    return false;
  }
  recipe = std::move(*parsed);
  return true;
}
}

int main(int argc, char *const *argv) {
  PlantRecipe recipe;
  PlantRecipeDemand demand;
  if (argc == 2 && std::string_view(argv[1]) == "--json") {
    if (!ReadDemand(demand) || !ReadJson(recipe)) { return 2; }
  } else if (argc != 1 || !ReadRecipe(recipe) || !ReadDemand(demand)) {
    return 2;
  }
  const auto size = MeasurePlantRecipe(recipe, demand);
  if (!size) {
    std::cerr << size.error();
    return 3;
  }
  const auto plan = BuildPlantPlan(recipe, demand);
  if (!plan) {
    std::cerr << plan.error();
    return 4;
  }
  std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
  std::cout << "M " << size->Root << ' ' << size->Axes << ' ' << size->Leaves << '\n';
  WriteRows(*plan, demand.MaximumLeaves);
}
