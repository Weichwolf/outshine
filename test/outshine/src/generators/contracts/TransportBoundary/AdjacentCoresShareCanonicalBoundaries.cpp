#include "TransportBoundary.h"
#include "Check.h"
#include <array>
#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  for (const int level : {0, 1, 8, GeoCellId::MaximumLevel}) {
    const uint32_t count = uint32_t{1} << static_cast<unsigned>(level);
    for (const uint32_t x : std::array<uint32_t, 3>{0, count / 2, count - 1}) {
      for (const uint32_t y : std::array<uint32_t, 3>{0, count / 2, count - 1}) {
        const GeoCellId core{.Level = level, .X = x, .Y = y};
        const GeoCellId east{.Level = level, .X = (x + 1) % count, .Y = y};
        const auto right = TransportBoundaryOf(core, TransportCoreSide::East);
        const auto left = TransportBoundaryOf(east, TransportCoreSide::West);
        CHECK(right && left && *right == *left && right->Valid(),
              "opposite sides share one identity, including the wrapped dateline");
        const auto south = TransportBoundaryOf(core, TransportCoreSide::South);
        const auto north = TransportBoundaryOf(core, TransportCoreSide::North);
        CHECK(south && north && south->Valid() && north->Valid() && *south != *north,
              "northern and southern boundaries remain distinct, including the root");
        CHECK(right && south && *right != *south,
              "longitude and latitude boundaries cannot collide at a corner");
        if (y + 1 < count) {
          const GeoCellId above{.Level = level, .X = x, .Y = y + 1};
          const auto below = TransportBoundaryOf(above, TransportCoreSide::South);
          CHECK(north && below && *north == *below,
                "adjacent latitude cores agree without requesting the neighboring plan");
        }
      }
    }
  }
  for (const auto core : std::array{GeoCellId{.Level = -1},
                                    GeoCellId{.Level = GeoCellId::MaximumLevel + 1},
                                    GeoCellId{.Level = 2, .X = 4},
                                    GeoCellId{.Level = 2, .Y = 4}}) {
    const auto boundary = TransportBoundaryOf(core, TransportCoreSide::East);
    CHECK(!boundary && boundary.error() == TransportBoundaryError::InvalidCore,
          "invalid geographic cells never enter boundary shifts or wrapping");
  }
  const auto side = TransportBoundaryOf({}, static_cast<TransportCoreSide>(255));
  CHECK(!side && side.error() == TransportBoundaryError::InvalidSide,
        "an invalid side remains a typed request error");
  CHECK((!TransportBoundary{.Level = -1}.Valid() && !TransportBoundary{.Level = 25}.Valid() &&
         !TransportBoundary{.Level = 1, .Line = 2}.Valid() &&
         !TransportBoundary{.Level = 1, .Segment = 2}.Valid() &&
         !TransportBoundary{.Axis = static_cast<TransportBoundaryAxis>(255)}.Valid()),
        "stored boundary values validate their axis and finite grid domain");
  return Report();
}
