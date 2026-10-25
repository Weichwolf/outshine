#include "TerrainInputPreparation.h"

#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "TerrainRefinement.h"

namespace outshine::Generators {

TerrainRefinementJob TerrainInputPreparation::BeginRefinement(
    const Patchwork &candidate,
    TerrainPageLayout layout,
    TerrainRefinementDetail detail,
    size_t maximumPatches,
    std::span<const TerrainRefinementCorridor> corridors) const {
  std::vector<TerrainRefinementSource> sources;
  sources.reserve(candidate.Sheets.size());
  for (const Sheet &sheet : candidate.Sheets) {
    const ::outshine::Ground::TerrainField *heights = nullptr;
    if (!sheet.Virtual && sheet.Side == layout.Side) { heights = FieldAt(sheet.Tile); }
    sources.push_back({.Page = &sheet, .Heights = heights});
  }
  return {sources, Frame_, layout, detail, maximumPatches, corridors};
}

bool TerrainInputPreparation::RefineByError(Patchwork &candidate,
                                            TerrainPageLayout layout,
                                            TerrainRefinementDetail detail,
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
