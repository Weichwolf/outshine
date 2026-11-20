#include "Check.h"
#include "TerrainRefinement.h"

#include <array>
#include <cstdint>
#include <memory>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;

  constexpr Data::TileId tile{.Zoom = 10, .X = 512, .Y = 512};
  const auto ridge = [] {
    Ground::TerrainField field(9, 9);
    for (uint32_t row = 0; row < 9; ++row) {
      field.SetM(row, 1, 4.0f);
      field.SetM(row, 2, 8.0f);
      field.SetM(row, 3, 4.0f);
    }
    return std::make_shared<const Ground::TerrainField>(std::move(field));
  }();
  const auto flat = std::make_shared<const Ground::TerrainField>(9, 9);
  TerrainRefinementCache previous(3);
  const auto select =
      [&](const std::shared_ptr<const Ground::TerrainField> &field, int side, double pxPerM) {
        const Sheet page{.Tile = tile, .Nodes = {}, .Side = side};
        TerrainRefinementCache current(side);
        const std::array sources{TerrainRefinementSource{
            .Page = &page, .Heights = field.get(), .Errors = current.Retain(field, previous)}};
        auto patches = RefineTerrain(sources,
                                     TangentFrame::At({}),
                                     {.Side = side, .Halo = 1},
                                     {.OrthographicPxPerM = pxPerM},
                                     16);
        previous = std::move(current);
        return patches;
      };

  const auto distant = select(ridge, 3, 0.01);
  CHECK(distant && distant->size() == 1, "the distant ridge is allowed one patch");
  const auto nearby = select(ridge, 3, 1.0);
  CHECK(nearby && nearby->size() > 1, "the same terrain refines when approached");
  const auto finerGrid = select(ridge, 5, 1.0);
  CHECK(finerGrid && finerGrid->size() == 1, "the finer page grid resolves the ridge itself");
  const auto coarserGrid = select(ridge, 3, 1.0);
  CHECK(coarserGrid && nearby && *coarserGrid == *nearby,
        "the coarser grid needs the extra patches");
  const auto replacement = select(flat, 3, 1.0);
  CHECK(replacement && replacement->size() == 1, "a replacement DEM at the same tile has no ridge");
  return Report();
}
