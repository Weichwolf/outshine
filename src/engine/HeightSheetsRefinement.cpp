#include "HeightSheets.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "TerrainRefinement.h"

namespace outshine {

bool HeightSheets::RefineByError(Patchwork &candidate,
                                 const Ground::GroundStream &ground,
                                 Generators::TerrainPageLayout layout,
                                 Generators::TerrainRefinementDetail detail,
                                 size_t maximumPatches,
                                 std::string &error) {
  std::vector<Generators::TerrainRefinementSource> sources;
  sources.reserve(candidate.Sheets.size());
  for (const Sheet &sheet : candidate.Sheets) {
    const Ground::TerrainField *heights = nullptr;
    if (!sheet.Virtual && sheet.Side == layout.Side) { heights = FieldAt(ground, sheet.Tile); }
    sources.push_back({.Page = &sheet, .Heights = heights});
  }
  auto refined = Generators::RefineTerrain(sources, Frame_, layout, detail, maximumPatches);
  if (!refined) {
    error = std::move(refined.error());
    return false;
  }
  candidate.Sheets = std::move(*refined);
  return true;
}

}
