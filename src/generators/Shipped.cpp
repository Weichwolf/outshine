#include "Shipped.h"

#include "ForestDraw.h"
#include "BuildingDraw.h"
#include "Buildings.h"
#include "Species.h"
#include <string_view>
#include <string>
#include <cstddef>
#include <vector>
#include <memory>
#include <utility>

namespace outshine::Generators {

bool Shipping::Stands(const outshine::Ground::VegetationTemplates &declared,
                      std::string_view speciesDir,
                      std::string &error) {
  if (Ready()) { return true; }
  if (!declared.Ready()) {
    error = "the declared vegetation carries no rows, so nothing shipped can stand on it";
    return false;
  }

  PerM2_.clear();
  for (size_t row = 0; row < declared.TemplateCount(); ++row) {
    PerM2_.push_back(declared.Rows()[row].Edge[2]);
  }

  std::vector<TreeSpecies> species;
  if (!ReadSpecies(std::string(speciesDir).c_str(), species, error)) { return false; }
  Stems_.clear();
  for (const TreeSpecies &one : species) {
    Forest::Stem stem;
    stem.HeightM = static_cast<double>(one.HeightM());
    Stems_.push_back(stem);
  }
  if (Stems_.empty() || PerM2_.empty()) {
    error = "the declaration names no species or no density, so nothing shipped can stand";
    return false;
  }

  auto made = std::make_unique<Forest>(Span<const Forest::Stem>(Stems_.data(), Stems_.size()),
                                       Span<const float>(PerM2_.data(), PerM2_.size()),
                                       declared.Limit());
  auto drawn = std::make_unique<ForestDraw>(ClusterId{0}, Stems_.front().HeightM);
  if (!Placing_.Add(Rank{0}, *made) || !Drawing_.Add(Rank{0}, *drawn)) {
    error = "the shipped catalogue names one rank twice";
    return false;
  }
  Made_.push_back(std::move(made));
  Draws_.push_back(std::move(drawn));

  auto built = std::make_unique<Buildings>(ContactMaterial{0});
  auto drawnBuilt = std::make_unique<BuildingDraw>(ClusterId{1}, 1.0);
  if (!Placing_.Add(Rank{1}, *built) || !Drawing_.Add(Rank{1}, *drawnBuilt)) {
    error = "the shipped catalogue names one rank twice";
    return false;
  }
  Made_.push_back(std::move(built));
  Draws_.push_back(std::move(drawnBuilt));
  return true;
}

} // namespace outshine::Generators
