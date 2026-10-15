#include "Shipped.h"
#include "Species.h"
#include "GroundSnapshot.h"
#include "GroundSample.h"
#include "RegionPool.h"
#include "Forest.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

Generators::Ground::Snapshot
Flat(const Tile &region, uint32_t row, const ::outshine::Ground::VegetationTemplates &templates) {
  std::array<GroundPatch::Posting, 4> samples;
  for (auto &sample : samples) { sample.Height = GroundSample::At(500); }
  auto grid = std::make_shared<ClassStructure::Grid>();
  grid->W = grid->H = 1;
  grid->OrgE = grid->OrgN = -1;
  grid->CellM = 2 * std::max(region.SpanEm(), region.SpanNm()) + 2;
  grid->Cells = {row | (1u << 8u), 0};
  Generators::Ground::Snapshot result;
  result.Patch = GroundPatch::Complete(region, 2, samples);
  result.Classes = std::make_shared<ClassStructure>(TangentFrame::At(region.Geo({0, 0})),
                                                    grid,
                                                    std::make_shared<const ClassStructure::Grid>(),
                                                    ClassStructure::FromRun{});
  result.Table = TableOf(templates);
  result.Features = FeatureField::Of({}, {}, {});
  return result;
}

void CheckStand(Shipping &catalogue,
                const Tile &region,
                const ::outshine::Ground::VegetationTemplates &templates,
                uint32_t row,
                const std::set<std::string_view> &allowed) {
  const auto ground = Generators::Ground::Of(region, Flat(region, row, templates));
  CHECK(ground.has_value(), "the actual declared ground row reaches a finite native snapshot");
  if (!ground) { return; }
  RegionPool pool({.Reached = region, .Anywhere = region}, {});
  auto lease = pool.TryAcquire(*ground);
  CHECK(lease.has_value(), "the ordinary native placement sink is available");
  if (!lease) { return; }
  std::set<std::string_view> used;
  for (size_t at = 0; at < catalogue.Placing().Count(); ++at) {
    const auto &generator = catalogue.Placing().At(at);
    if (generator.Called() != "flora") { continue; }
    std::vector<Yield::Note> notes(generator.NoteNames().size());
    Yield yield(lease->Sink(), generator.NoteNames(), notes);
    generator.Occupy(*ground, yield);
    const auto bodies = lease->Sink().Placed();
    CHECK(!bodies.empty(), "positive source density still creates the native stand");
    for (const auto &body : bodies) {
      const auto *species = catalogue.TreeFor(ClusterId{body.Variant});
      CHECK(species && allowed.contains(species->Name()),
            "every actual body belongs to the row's declared species, never the whole catalogue");
      if (species) { used.insert(species->Name()); }
    }
  }
  CHECK(used.size() > 1, "a declared mixture retains more than one species in the actual stand");
}

void RejectBadRecipes(Shipping &catalogue,
                      const ::outshine::Ground::VegetationTemplates &templates) {
  std::string directory =
      (std::filesystem::temp_directory_path() / "outshine-forest-mixtures-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "invalid recipe tests own a temporary directory");
  const auto path = std::filesystem::path(directory) / "invalid.json";
  const auto *retained = catalogue.TreeFor(ClusterId{0});
  const std::array recipes{
      R"({"templates":[{"name":"mixed_broadleaf","trees":{"species":{"missing_species":1}}}]})",
      R"({"templates":[{"name":"mixed_broadleaf","trees":{"species":{"beech":-1}}}]})",
      R"({"templates":[{"name":"mixed_broadleaf","trees":{"species":{"beech":"1"}}}]})",
      R"({"templates":[{"name":"mixed_broadleaf","trees":{"species":{"beech":1,"beech":2}}}]})"};
  for (const auto *recipe : recipes) {
    std::ofstream(path) << recipe;
    std::string error;
    CHECK(!catalogue.EnsureCatalogue(
              templates, "src/assets/world/species", error, true, path.string()) &&
              !error.empty(),
          "unknown, negative, nonnumeric and duplicate species weights are rejected");
    CHECK(catalogue.Ready() && catalogue.TreeFor(ClusterId{0}) == retained,
          "a rejected recipe preserves the preceding catalogue and borrowed species");
  }
  std::filesystem::remove_all(directory);
}

void CheckEmptySupport(const Tile &region,
                       const ::outshine::Ground::VegetationTemplates &templates) {
  const auto ground = Generators::Ground::Of(region, Flat(region, 0, templates));
  CHECK(ground.has_value(), "zero-support control uses the same valid ground");
  if (!ground) { return; }
  const std::array<Forest::Stem, 1> stems{};
  const std::vector<float> density(templates.TemplateCount(), 1);
  const Forest forest(
      stems, density, AlpineLimit{}, std::vector<SpeciesMixture>(templates.TemplateCount()));
  RegionPool pool({.Reached = region, .Anywhere = region}, {});
  auto lease = pool.TryAcquire(*ground);
  CHECK(lease.has_value(), "zero-support control obtains an ordinary placement sink");
  if (!lease) { return; }
  std::vector<Yield::Note> notes(forest.NoteNames().size());
  Yield yield(lease->Sink(), forest.NoteNames(), notes);
  forest.Occupy(*ground, yield);
  CHECK(lease->Sink().Placed().empty(),
        "explicit empty species support creates no bodies despite positive density");
}
}

int main() {
  using namespace outshine::Test;
  ::outshine::Ground::GroundMaterials materials;
  ::outshine::Ground::VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "source materials load");
  CHECK(templates.Load("src/assets/world/vegetation.json", materials), "source templates load");
  Shipping catalogue;
  std::string error;
  CHECK(catalogue.EnsureCatalogue(
            templates, "src/assets/world/species", error, true, "src/assets/world/vegetation.json"),
        "the actual world catalogue binds the provided source mixtures");
  if (!catalogue.Ready()) { return Report(); }
  const auto region = Tile::Of(16, {.LongitudeDeg = 10, .LatitudeDeg = 47});
  CheckStand(catalogue,
             region,
             templates,
             0,
             {"beech", "oak", "hornbeam", "ash", "sycamore", "birch", "elm"});
  CheckStand(catalogue, region, templates, 2, {"oak", "lime", "ash"});
  RejectBadRecipes(catalogue, templates);
  CheckEmptySupport(region, templates);
  return Report();
}
