#include "TerrainInputPreparation.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "TerrainRefinement.h"

namespace outshine::Generators {

TerrainRefinementJob
TerrainInputPreparation::BeginRefinement(const Patchwork &candidate,
                                         TerrainPageLayout layout,
                                         TerrainRefinementDetail detail,
                                         size_t maximumPatches,
                                         std::span<const TerrainRefinementCorridor> corridors,
                                         TerrainRefinementCache *cache) const {
  std::vector<TerrainRefinementSource> sources;
  sources.reserve(candidate.Sheets.size());
  TerrainRefinementCache retained(layout.Side);
  for (const Sheet &sheet : candidate.Sheets) {
    const auto *entry =
        !sheet.Virtual && sheet.Side == layout.Side ? FindField(sheet.Tile) : nullptr;
    const auto *heights = entry != nullptr ? entry->second.get() : nullptr;
    std::shared_ptr<TerrainPatchErrors> errors;
    if (cache != nullptr && heights != nullptr && heights->Meshable() &&
        std::cmp_greater(std::max(heights->Rows(), heights->Cols()), layout.Side)) {
      errors = retained.Retain(entry->second, *cache);
    }
    sources.push_back({.Page = &sheet, .Heights = heights, .Errors = std::move(errors)});
  }
  if (cache != nullptr) { *cache = std::move(retained); }
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
