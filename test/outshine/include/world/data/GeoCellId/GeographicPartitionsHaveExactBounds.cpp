#include <world/data/Address.h>
#include <world/data/GeoCellId.h>
#include "Check.h"

#include <array>
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  const auto world = GeoCellId{}.Bounds();
  CHECK(world && world->WestDeg == -180 && world->SouthDeg == -90 && world->EastDeg == 180 &&
            world->NorthDeg == 90,
        "level zero represents the geographic world including both poles");
  const GeoCellId flensburg{.Level = 9, .X = 269, .Y = 411};
  const auto bounds = flensburg.Bounds();
  CHECK(bounds && bounds->WestDeg == 9.140625 && bounds->SouthDeg == 54.4921875 &&
            bounds->EastDeg == 9.84375 && bounds->NorthDeg == 54.84375,
        "known geographic source cell bounds are independent of Mercator render tiles");
  const Address address = Address::AtGeoCell(flensburg);
  CHECK(address.How() == Scheme::GeodeticGrid && address.GeoCell() == flensburg &&
            !address.Tile() && !address.Cell() && !address.Index() &&
            address.Text() == "q/9/269/411" && address == Address::AtGeoCell(flensburg),
        "cell level and axes retain a distinct owned source-address identity");
  CHECK(Address::At({.Zoom = 17, .X = 69, .Y = 45}).Text() == "17/69/45" &&
            Address::Whole(123).Text() == "w/123" &&
            Address::AtCell({.SouthDeg = -34, .WestDeg = 18}).Text() == "g/-34/18",
        "existing Mercator, indexed and one-degree source-cache names are preserved");

  for (int level = 0; level < GeoCellId::MaximumLevel; ++level) {
    const uint32_t last = (uint32_t{1} << level) - 1;
    const auto parent = GeoCellId{.Level = level, .X = last, .Y = last}.Bounds();
    double total = 0;
    for (uint32_t dy = 0; dy < 2; ++dy) {
      for (uint32_t dx = 0; dx < 2; ++dx) {
        const auto child =
            GeoCellId{.Level = level + 1, .X = 2 * last + dx, .Y = 2 * last + dy}.Bounds();
        CHECK(parent && child && child->WestDeg >= parent->WestDeg &&
                  child->SouthDeg >= parent->SouthDeg && child->EastDeg <= parent->EastDeg &&
                  child->NorthDeg <= parent->NorthDeg,
              "all refinements stay inside their geographic parent through the finest level");
        if (child) {
          total += (child->EastDeg - child->WestDeg) * (child->NorthDeg - child->SouthDeg);
        }
      }
    }
    CHECK(parent && parent->EastDeg == 180 && parent->NorthDeg == 90 &&
              total == (parent->EastDeg - parent->WestDeg) * (parent->NorthDeg - parent->SouthDeg),
          "four children partition the parent exactly at the antimeridian and north pole");
  }
  const std::array invalid{GeoCellId{.Level = -1},
                           GeoCellId{.Level = 25},
                           GeoCellId{.Level = std::numeric_limits<int>::max()},
                           GeoCellId{.Level = 9, .X = 512},
                           GeoCellId{.Level = 9, .Y = 512},
                           GeoCellId{.Level = 24, .X = std::numeric_limits<uint32_t>::max()}};
  for (const auto &cell : invalid) {
    CHECK(!cell.Valid() && !cell.Bounds(),
          "invalid source coordinates are rejected without wrapping, clipping or invalid shifts");
  }
  return Report();
}
