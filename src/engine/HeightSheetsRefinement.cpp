#include "HeightSheets.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "TerrainRefinement.h"

namespace outshine {

Generators::TerrainRefinementJob
HeightSheets::BeginRefinement(const Patchwork &candidate,
                              Generators::TerrainPageLayout layout,
                              Generators::TerrainRefinementDetail detail,
                              size_t maximumPatches) const {
  std::vector<Generators::TerrainRefinementSource> sources;
  sources.reserve(candidate.Sheets.size());
  for (const Sheet &sheet : candidate.Sheets) {
    const Ground::TerrainField *heights = nullptr;
    if (!sheet.Virtual && sheet.Side == layout.Side) { heights = FieldAt(sheet.Tile); }
    sources.push_back({.Page = &sheet, .Heights = heights});
  }
  return {sources, Frame_, layout, detail, maximumPatches};
}

bool HeightSheets::RefineByError(Patchwork &candidate,
                                 Generators::TerrainPageLayout layout,
                                 Generators::TerrainRefinementDetail detail,
                                 size_t maximumPatches,
                                 std::string &error) const {
  auto job = BeginRefinement(candidate, layout, detail, maximumPatches);
  auto refined = job.Advance(candidate.Sheets.size());
  if (!refined) {
    error = std::move(refined.error());
    return false;
  }
  if (!*refined) {
    error = "terrain refinement did not consume its declared source budget";
    return false;
  }
  candidate.Sheets = std::move(job).Take();
  return true;
}

}
