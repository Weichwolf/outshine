#include "GroundTileUpload.h"
#include "Check.h"
#include <limits>
#include <type_traits>

using namespace outshine;
static_assert(!std::is_convertible_v<Render::HeightPageHandle, Render::PieceHandle>);
static_assert(!std::is_convertible_v<Render::PieceHandle, Render::HeightPageHandle>);
static_assert(!std::is_convertible_v<Render::HeightPageHandle, float>);

int main() {
  using namespace outshine::Test;
  Core::GroundTile tile;
  tile.Row[12] = 7;
  tile.Corners = {{-1, -2, 3, -2, -1, 4, 3, 4}};
  tile.Page = {.Slot = 9, .Generation = std::numeric_limits<uint64_t>::max()};
  tile.SagInv = 0.125f;
  tile.StepE = 2;
  tile.StepN = 3;
  tile.LowM = -4;
  tile.HighM = 8;
  constexpr uint32_t boundary = uint32_t{1} << 24;
  for (const uint32_t resident : {uint32_t{0}, boundary - 1, boundary, boundary + 2}) {
    const auto encoded = Core::EncodeGroundTile(tile, resident);
    CHECK(encoded && static_cast<double>(encoded->Instance.Page) == resident,
          "exact GPU addresses preserve their integer value at the shader boundary");
    CHECK(encoded && encoded->Instance.Row == tile.Row &&
              encoded->Instance.Corners == tile.Corners &&
              encoded->Instance.SagInv == tile.SagInv && encoded->Instance.StepE == tile.StepE &&
              encoded->Instance.StepN == tile.StepN && encoded->LowM == tile.LowM &&
              encoded->HighM == tile.HighM,
          "the adapter preserves native geometry independently of native resource identity");
  }
  CHECK(!Core::EncodeGroundTile(tile, boundary + 1), "inexact address cannot alias its neighbor");
  CHECK(!Core::EncodeGroundTile(tile, Render::kNoPage), "invalid GPU address is rejected");
  CHECK(!Core::EncodeGroundTile(tile, std::numeric_limits<uint32_t>::max() - 1),
        "rounding near uint32 maximum never performs a float-to-integer conversion");
  return Report();
}
