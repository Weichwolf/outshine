#include "TerrainLoader.h"
#include "TerrainSamplingCoverage.h"
#include "SourceSet.h"
#include "Check.h"
#include "Address.h"
#include "ContentStore.h"
#include "TilePool.h"
#include "Transport.h"

#include <array>
#include <cstdint>
#include <string>

using namespace outshine;

namespace {
class NoNetwork final : public Data::Transport {
public:
  int Starts = 0;

  Data::Ticket Begin(const std::string &) override {
    ++Starts;
    return Data::Ticket::None;
  }

  Data::Wire Collect(Data::Ticket) override { return Data::Wire::Never(); }

  void Cancel(Data::Ticket) override {}
};

constexpr auto kCompiledCoverage =
    Ground::TerrainSamplingCoverage::ForField({.Zoom = 6, .X = 25, .Y = 25});
static_assert(kCompiledCoverage && kCompiledCoverage->Coarse &&
              kCompiledCoverage->Coarse->Zoom == 3 && kCompiledCoverage->Coarse->X == 3 &&
              kCompiledCoverage->Coarse->Y == 3);
static_assert(!Ground::TerrainSamplingCoverage::ForField({.Zoom = -1, .X = 0, .Y = 0}));
}

int main() {
  using namespace outshine::Test;
  for (int zoom = 0; zoom <= 30; ++zoom) {
    const uint32_t side = uint32_t{1} << static_cast<uint32_t>(zoom);
    for (const uint32_t x : std::array{0u, side / 2u, side - 1u}) {
      const uint32_t y = side - 1u - x;
      const Data::TileId fine{.Zoom = zoom, .X = x, .Y = y};
      const auto coverage = Ground::TerrainSamplingCoverage::ForField(fine);
      CHECK(coverage && coverage->Fine == fine,
            "sampling preserves the exact native field at every supported source zoom");
      CHECK(coverage && coverage->Coarse.has_value() == (zoom > 3),
            "fallback exists only when its positive source zoom can be represented");
      if (coverage && coverage->Coarse) {
        CHECK(coverage->Coarse->Zoom == zoom - 3 && coverage->Coarse->X == x / 8u &&
                  coverage->Coarse->Y == y / 8u,
              "independent integer cell division proves the fallback contains the fine tile");
      }
    }
  }
  for (const auto tile : {Data::TileId{.Zoom = 31, .X = 0, .Y = 0},
                          {.Zoom = -1, .X = 0, .Y = 0},
                          {.Zoom = 4, .X = 16, .Y = 0},
                          {.Zoom = 4, .X = 0, .Y = 16}}) {
    CHECK(!Ground::TerrainSamplingCoverage::ForField(tile),
          "invalid native grids are refused before shifting coordinates");
  }

  Data::ContentStore store({.Using = Data::ContentStore::Use::Off});
  Data::SourceSet sources(store);
  NoNetwork network;
  Ground::TilePool pool({}, sources, network);
  Ground::GroundStream ground(pool, {.Z = 6, .Grid = 4});
  const auto converted = ground.SamplingCoverage({.Zoom = 9, .X = 200, .Y = 201});
  const Data::TileId fine{.Zoom = 6, .X = 25, .Y = 25};
  const Data::TileId coarse{.Zoom = 3, .X = 3, .Y = 3};
  CHECK(converted && converted->Fine == fine && converted->Coarse == coarse,
        "source coverage uses the sampler's configured grid before deriving its fallback");
  const auto native = ground.SamplingCoverage(fine);
  CHECK(native && converted && native->Fine == converted->Fine &&
            native->Coarse == converted->Coarse,
        "a source tile's zoom does not change the normal and coarse sampler address set");
  CHECK(!ground.SamplingCoverage({.Zoom = 5, .X = 0, .Y = 0}) &&
            !ground.SamplingCoverage({.Zoom = 31, .X = 0, .Y = 0}) &&
            !ground.SamplingCoverage({.Zoom = 9, .X = 512, .Y = 0}),
        "coarse or invalid inputs cannot manufacture a more precise sampling address");
  CHECK(pool.Counters().Posts == 0 && pool.Counters().Fetches == 0 && network.Starts == 0,
        "coverage planning allocates no jobs and starts no provider or network work");
  return Report();
}
