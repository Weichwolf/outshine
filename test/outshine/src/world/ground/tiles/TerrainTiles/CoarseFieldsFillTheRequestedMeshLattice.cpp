#include "TerrainTiles.h"
#include "Check.h"
#include <vector>

namespace {
using namespace outshine;

class Coarse final : public Ground::TerrainSource {
public:
  Ground::TerrainBytes Take(Data::TileId at) override {
    Ground::TerrainField field(2, 2);
    for (uint32_t row = 0; row < 2; ++row) {
      for (uint32_t col = 0; col < 2; ++col) {
        field.SetM(row, col, static_cast<float>((at.X + col) * (at.Y + row)));
      }
    }
    return Ground::TerrainBytes::From(at, std::move(field), {.Tile = at});
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Coarse source;
  Ground::TerrainTiles terrain(source, Ground::EnuFrame::At({}), {});
  const Data::TileId at{.Zoom = 3, .X = 2, .Y = 2};
  std::vector<float> heights;
  std::vector<Data::TileSourceIdentity> identities;
  int side = 0;
  uint32_t postings = 0;
  CHECK(terrain.SampleNodeHeights(at, 64, &heights, &identities, &postings, &side) ==
            Ground::TerrainGrid::State::Decoded,
        "a valid coarse source produces a renderable requested mesh");
  CHECK(side == 65 && postings == 65 && heights.size() == 65 * 65,
        "source resolution never shrinks the renderer's required lattice");
  if (heights.size() == 65 * 65) {
    for (uint32_t row = 0; row < 65; ++row) {
      for (uint32_t col = 0; col < 65; ++col) {
        const double expected =
            (2.0 + static_cast<double>(col) / 64.0) * (2.0 + static_cast<double>(row) / 64.0);
        CHECK_NEAR(heights[row * 65 + col],
                   expected,
                   0.0,
                   "m",
                   "interpolation preserves the independent bilinear surface at every mesh node");
      }
    }
  }
  return Report();
}
