#include "Forest.h"
#include "GroundSnapshot.h"
#include "GroundSample.h"
#include "RegionPool.h"
#include "ReadTextFile.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
using Stand = std::vector<std::tuple<double, double, double, uint32_t>>;

::outshine::Ground::VegetationTemplates Templates(const std::filesystem::path &file,
                                                  int exposureDeg) {
  const auto original = ReadTextFile("src/assets/world/ground-materials.json", 1024 * 1024);
  CHECK(original.has_value(), "the counterexample uses the shipped material definitions");
  ::outshine::Ground::VegetationTemplates templates;
  if (!original) { return templates; }
  std::string text = *original;
  const auto forest = text.find("\"name\": \"forest_floor\"");
  const auto exposure = text.find("\"plausibleDeg\"", forest);
  const auto first = text.find_first_of("0123456789", text.find(',', exposure));
  const auto last = text.find_first_not_of("0123456789", first);
  CHECK(first != std::string::npos && last != std::string::npos,
        "the forest material has an explicit numeric exposure threshold");
  if (first == std::string::npos || last == std::string::npos) { return templates; }
  text.replace(first, last - first, std::to_string(exposureDeg));
  std::ofstream(file) << text;
  ::outshine::Ground::GroundMaterials materials;
  CHECK(materials.Load(file.c_str()), "only the material exposure threshold changes");
  CHECK(templates.Load("src/assets/world/vegetation.json", materials),
        "the identical source habitats consume the changed material");
  return templates;
}

Stand Place(const ::outshine::Ground::VegetationTemplates &templates,
            uint32_t row,
            double baseAslM = 500) {
  const Tile region = Tile::Of(18, {.LongitudeDeg = 10, .LatitudeDeg = 47});
  constexpr int kSide = 8;
  std::array<GroundPatch::Posting, kSide * kSide> samples;
  for (int j = 0; j < kSide; ++j) {
    for (int i = 0; i < kSide; ++i) {
      samples[static_cast<size_t>(j * kSide + i)].Height =
          GroundSample::At(baseAslM + region.SpanEm() * static_cast<double>(i) /
                                          static_cast<double>(kSide - 1) * std::tan(40 * kDeg2Rad));
    }
  }
  auto grid = std::make_shared<ClassStructure::Grid>();
  grid->W = grid->H = 1;
  grid->OrgE = grid->OrgN = -1;
  grid->CellM = 2 * std::max(region.SpanEm(), region.SpanNm()) + 2;
  grid->Cells = {row | (1u << 8u), 0};
  Generators::Ground::Snapshot snapshot;
  snapshot.Patch = GroundPatch::Complete(region, kSide, samples);
  snapshot.Table = TableOf(templates);
  snapshot.Classes =
      std::make_shared<ClassStructure>(TangentFrame::At(region.Geo({0, 0})),
                                       grid,
                                       std::make_shared<const ClassStructure::Grid>(),
                                       ClassStructure::FromRun{});
  snapshot.Features = FeatureField::Of({}, {}, {});
  const auto ground = Generators::Ground::Of(region, snapshot);
  CHECK(ground.has_value(), "source habitat and sloping native ground form a complete snapshot");
  if (!ground) { return {}; }
  CHECK(std::abs(ground->SlopeDeg({region.SpanEm() / 2, region.SpanNm() / 2}) - 40) < 1e-4,
        "the actual native sampler sees the forty-degree incline");
  std::vector<float> density(templates.TemplateCount(), 0);
  density[0] = .028f;
  const Forest forest(Forest::Stem{}, density, templates.Limit());
  RegionPool pool({.Reached = region, .Anywhere = region}, {});
  auto lease = pool.TryAcquire(*ground);
  CHECK(lease.has_value(), "the actual forest generator receives a native placement sink");
  if (!lease) { return {}; }
  std::vector<Yield::Note> notes(forest.NoteNames().size());
  Yield yield(lease->Sink(), forest.NoteNames(), notes);
  forest.Occupy(*ground, yield);
  Stand placed;
  for (const auto &body : lease->Sink().Placed()) {
    placed.emplace_back(body.Em, body.Nm, body.BaseAslM, body.Variant);
  }
  return placed;
}
}

int main() {
  std::string directory =
      (std::filesystem::temp_directory_path() / "outshine-forest-exposure-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "the material variants own their temporary files");
  const auto early = Templates(std::filesystem::path(directory) / "early.json", 5);
  const auto late = Templates(std::filesystem::path(directory) / "late.json", 80);
  if (early.Ready() && late.Ready()) {
    CHECK(early.Rows()[0].Edge[3] != late.Rows()[0].Edge[3],
          "the rendered ground exposure really changes");
    const auto before = Place(early, 0);
    const auto after = Place(late, 0);
    CHECK(!before.empty() && !after.empty(), "source-supported forest survives both materials");
    CHECK(before == after, "a visual exposure parameter never changes source-supported stands");
    CHECK(Place(early, 6).empty() && Place(late, 6).empty(),
          "rock without declared tree support stays empty at either material exposure");
    CHECK(Place(early, 0, 3000).empty() && Place(late, 0, 3000).empty(),
          "the independent native treeline remains binding for supported forest");
  }
  std::filesystem::remove_all(directory);
  return Report();
}
