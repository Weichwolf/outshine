#include "OsmField.h"
#include "Check.h"

#include <array>
#include <memory>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  std::shared_ptr<const OsmField> pinned;
  uint64_t generation = 0;
  {
    const std::array<std::string, 1> layers{"building"};
    OsmField source(14, layers);
    const std::array<OsmField::Declared, 1> first{
        {{.Layer = "building",
          .Key = "kind",
          .Value = "hall",
          .HeightM = 9,
          .Area = true,
          .LatLon = {47.0, 9.0, 47.0, 9.001, 47.001, 9.001, 47.001, 9.0}}}};
    source.Declare(first, TileAt{.X = 8581, .Y = 5603});
    generation = source.Generation();
    pinned = source.SnapshotQueries();
    CHECK(pinned && pinned->Features().size() == 1 && pinned->Rings().size() == 1 &&
              pinned->Points().size() == 8 && pinned->Tiles().size() == 1,
          "the query snapshot owns complete feature geometry and tile identity");
    CHECK(pinned && pinned->Str(pinned->Features().front(), "kind") == "hall" &&
              pinned->Num(pinned->Features().front(), "height", 0) == 9.0 &&
              pinned->Integer(pinned->Features().front(), "height") == 9,
          "the query snapshot retains string and numeric tags");
    auto changed = first;
    changed.front().Value = "tower";
    changed.front().HeightM = 24;
    source.Declare(changed, TileAt{.X = 8582, .Y = 5603});
    CHECK(source.Generation() != generation && source.Tiles().front().X == 8582 &&
              source.Str(source.Features().front(), "kind") == "tower",
          "a new source generation changes the ingest owner");
    CHECK(pinned->Generation() == generation && pinned->Tiles().front().X == 8581 &&
              pinned->Str(pinned->Features().front(), "kind") == "hall" &&
              pinned->Num(pinned->Features().front(), "height", 0) == 9.0 &&
              pinned->Points().data() != source.Points().data(),
          "a changed ingest owner cannot mutate the pinned query generation");
  }
  CHECK(pinned && pinned->Generation() == generation && pinned->Features().size() == 1 &&
            pinned->Str(pinned->Features().front(), "kind") == "hall" &&
            pinned->Points()[3] == 9.001,
        "the published query data remains valid after the ingest owner is destroyed");
  return Report();
}
