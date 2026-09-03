#include <span>
#include "Shipped.h"

#include "BuildingMesh.h"
#include "Forest.h"
#include "GroundPatchwork.h"
#include "Structures.h"
#include "RoadMesh.h"

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

Shipping::Shipping()
    : Offered_(std::make_unique<Structures>()),
      Coverer_(std::make_unique<Patchworker>()),
      Shaper_(std::make_unique<BuildingMesh>()),
      Paver_(std::make_unique<RoadMesh>()) {}

Shipping::~Shipping() = default;

namespace {

constexpr Rank kRankBuilding{100};
constexpr Rank kRankFlora{200};

} // namespace

bool Shipping::Stands(const outshine::Ground::VegetationTemplates &declared,
                      std::string_view speciesDir,
                      std::string &error) {
  if (Ready()) { return true; }
  if (!declared.Ready()) {
    error = "the declared vegetation carries no rows, so nothing shipped can stand on it";
    return false;
  }

  std::vector<float> perM2;
  perM2.reserve(declared.TemplateCount());
  for (size_t row = 0; row < declared.TemplateCount(); ++row) {
    perM2.push_back(declared.Rows()[row].Edge[2]);
  }

  std::vector<TreeSpecies> species;
  if (!ReadSpecies(std::string(speciesDir).c_str(), species, error)) { return false; }
  std::vector<Forest::Stem> stems;
  stems.reserve(species.size());
  for (const TreeSpecies &one : species) {
    stems.push_back({.HeightM = static_cast<double>(one.HeightM())});
  }
  if (stems.empty() || perM2.empty()) {
    error = "the declaration names no species or no density, so nothing shipped can stand";
    return false;
  }

  auto made = std::make_unique<Forest>(std::span<const Forest::Stem>(stems.data(), stems.size()),
                                       std::span<const float>(perM2.data(), perM2.size()),
                                       declared.Limit());
  auto drawn = std::make_unique<ForestDraw>(ClusterId{0}, stems.front().HeightM);
  if (!Placing_.Add(kRankFlora, *made) || !Drawing_.Add(kRankFlora, *drawn)) {
    error = "the shipped catalogue names one rank twice";
    return false;
  }
  Made_.push_back(std::move(made));
  Draws_.push_back(std::move(drawn));

  auto built = std::make_unique<Buildings>(ContactMaterial{0});
  auto drawnBuilt = std::make_unique<BuildingDraw>(ClusterId{1}, 1.0);
  if (!Placing_.Add(kRankBuilding, *built) || !Drawing_.Add(kRankBuilding, *drawnBuilt)) {
    error = "the shipped catalogue names one rank twice";
    return false;
  }
  Made_.push_back(std::move(built));
  Draws_.push_back(std::move(drawnBuilt));
  return true;
}

} // namespace outshine::Generators
