#include "Check.h"
#include "TerrainMesh.h"

#include <cstdint>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;

  constexpr int zoom = 20;
  constexpr uint32_t centre = 1u << (zoom - 1);
  constexpr TerrainPageLayout layout{.Side = 2, .Halo = 1};
  Sheet sheet{.Tile = {.Zoom = zoom, .X = centre, .Y = centre},
              .Nodes = std::vector<float>(layout.NodeCount(), -50.0f),
              .Side = layout.Side,
              .Postings = 2,
              .Virtual = true};
  sheet.Nodes[layout.NodeAt(0, 0)] = 2.0f;
  sheet.Nodes[layout.NodeAt(1, 0)] = 4.0f;
  sheet.Nodes[layout.NodeAt(0, 1)] = 6.0f;
  sheet.Nodes[layout.NodeAt(1, 1)] = 8.0f;
  Patchwork candidate;
  candidate.Sheets.push_back(std::move(sheet));

  const TerrainMesh mesh = BuildTerrainMesh(candidate, TangentFrame::At({}), layout);
  const std::vector<uint32_t> expectedIndices = {0, 2, 3, 0, 3, 1};
  CHECK(mesh.PositionsM.size() == 12 && mesh.Indices == expectedIndices,
        "one terrain quad has the native vertex and CCW triangle layout");
  CHECK(mesh.LowestM == 2.0 && mesh.TallestM == 8.0 && mesh.TallestDistanceM > 0.0,
        "range diagnostics use visible nodes and ignore the halo");
  CHECK(mesh.PositionsM[1] < mesh.PositionsM[4] && mesh.PositionsM[4] < mesh.PositionsM[10],
        "native positions preserve increasing source heights");

  const TerrainMesh filtered = BuildTerrainMesh(candidate, TangentFrame::At({}), layout, zoom + 1);
  CHECK(filtered.PositionsM.empty() && filtered.Indices.empty() && filtered.LowestM == 0.0 &&
            filtered.TallestM == 0.0,
        "minimum zoom filters the complete sheet without sentinel ranges");
  const TerrainMesh invalid =
      BuildTerrainMesh(candidate, TangentFrame::At({}), {.Side = 1, .Halo = 0});
  CHECK(invalid.PositionsM.empty() && invalid.Indices.empty(),
        "an invalid page layout produces no partial mesh");
  return Report();
}
