#include "Check.h"
#include "TerrainMesh.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;

  constexpr TerrainPageLayout layout{.Side = 33, .Halo = 1};
  constexpr int zoom = 15;
  constexpr uint32_t centre = 1u << (zoom - 1);
  Patchwork terrain;
  for (uint32_t page = 0; page < 3; ++page) {
    Sheet sheet{.Tile = {.Zoom = zoom, .X = centre + page, .Y = centre},
                .Nodes = std::vector<float>(layout.NodeCount(), -1000.0f),
                .Side = layout.Side,
                .Postings = 257,
                .Virtual = page != 1};
    for (int row = 0; row < layout.Side; ++row) {
      for (int column = 0; column < layout.Side; ++column) {
        sheet.Nodes[layout.NodeAt(column, row)] =
            static_cast<float>(row * column - 500) + static_cast<float>(page);
      }
    }
    terrain.Sheets.push_back(std::move(sheet));
  }
  const auto frame = TangentFrame::At({});
  const TerrainMesh reference = BuildTerrainMesh(terrain, frame, layout);
  for (size_t stride : std::array<size_t, 5>{1, 7, 16, 1089, 4096}) {
    TerrainSurvey survey;
    for (const Sheet &sheet : terrain.Sheets) {
      SurveyTerrainSheet(survey, sheet, frame, layout, stride);
    }
    CHECK(survey.Vertices == reference.PositionsM.size() / 3 &&
              survey.LowestM == reference.LowestM && survey.TallestM == reference.TallestM &&
              survey.TallestDistanceM == reference.TallestDistanceM,
          "survey keeps relief extrema and first tallest node distance, excluding halo");
    CHECK(survey.ProbePositionsM.size() == (survey.Vertices + stride - 1) / stride,
          "only demanded probes are retained across sheet boundaries");
    for (size_t probe = 0; probe < survey.ProbePositionsM.size(); ++probe) {
      const size_t first = probe * stride * 3;
      const Vec3f expected = {{reference.PositionsM[first],
                               reference.PositionsM[first + 1],
                               reference.PositionsM[first + 2]}};
      CHECK(survey.ProbePositionsM[probe] == expected,
            "survey probes exactly match strided legacy mesh positions without index output");
    }
  }

  TerrainSurvey refused;
  SurveyTerrainSheet(refused, terrain.Sheets.front(), frame, layout, 0);
  SurveyTerrainSheet(refused, terrain.Sheets.front(), frame, {.Side = 1, .Halo = 0}, 16);
  Sheet invalid = terrain.Sheets.front();
  invalid.Nodes.pop_back();
  SurveyTerrainSheet(refused, invalid, frame, layout, 16);
  invalid = terrain.Sheets[1];
  invalid.Postings = 1;
  SurveyTerrainSheet(refused, invalid, frame, layout, 16);
  CHECK(refused.Vertices == 0 && refused.ProbePositionsM.empty() && refused.TallestM == 0 &&
            refused.LowestM == 0,
        "invalid layouts, missing nodes and zero stride do not produce partial surveys");
  return Report();
}
