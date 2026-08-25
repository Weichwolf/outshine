#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

#include "Check.h"

#include "VectorTileMaker.h"

#include "OsmField.h"
#include "OsmLayer.h"

using outshine::Ground::OsmField;
using outshine::Ground::OsmLayer;
using outshine::Ground::OsmLayerNames;

namespace {

// board:1806: OsmField is the entry point of the whole ground layer -- RoadHarvest, StreetField,
// WaterField, BuildingField, ClassField and World all read it -- and nothing under test/ named
// it. It was untestable for a structural reason rather than an oversight: its only door took a
// TilePool, which means threads, a content store and fetched bytes.
//
// board:1806 splits that door. Build(TilePool&) now fetches and calls Accept(tx, ty, bytes),
// which is a pure function of the bytes. So this twin hand-encodes a Mapbox Vector Tile -- the
// format is protobuf and a tile with one layer, one feature and two tags is under a hundred
// bytes -- and proves the decode and the field's answering doors together.
//
// Encoding, from the MVT 2.1 specification:
//   Tile    { repeated Layer layers = 3 }
//   Layer   { string name = 1, repeated Feature features = 2, repeated string keys = 3,
//             repeated Value values = 4, uint32 extent = 5, uint32 version = 15 }
//   Feature { uint64 id = 1, packed uint32 tags = 2, GeomType type = 3, packed uint32 geometry = 4 }
//   Value   { string string_value = 1, double double_value = 3 }
// Geometry is a stream of command integers (id | count << 3) and zig-zag parameters.
// the tile's own corner, from the standard web-mercator formulas rather than from the code
// under test -- otherwise the check would be the decoder agreeing with itself.
struct Corner {
  double LatDeg = 0.0, LonDeg = 0.0;
};

[[nodiscard]] Corner CornerOf(int z, double x, double y) {
  const double side = std::exp2((double)z);
  const double n = std::numbers::pi * (1.0 - 2.0 * y / side);
  return Corner{std::atan(std::sinh(n)) * 180.0 / std::numbers::pi, x / side * 360.0 - 180.0};
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  constexpr int kZoom = 12;
  constexpr int kTileX = 2179, kTileY = 1421;
  constexpr uint32_t kExtent = 4096;

  const std::vector<std::string> layers =
      OsmLayerNames({OsmLayer::Buildings, OsmLayer::WaterPolygons, OsmLayer::WaterLines,
                     OsmLayer::Streets, OsmLayer::StreetPolygons});
  OsmField field(kZoom, layers);

  Note("layers the field is declared with", (double)layers.size(), "layers");
  CHECK(field.Layer(OsmLayer::Streets) == 3 && field.Layer(OsmLayer::Buildings) == 0,
        "the layer table is the declaration's own order, so a caller naming a layer by its "
        "enum reaches the same column the tile was parsed into");
  CHECK(field.Layer("no_such_layer") < 0,
        "and a layer nobody declared resolves to no column rather than to column zero");
  CHECK(field.Features().empty() && !field.Settled(kTileX, kTileY),
        "and a field that has accepted nothing holds nothing and says the tile is not decoded");

  namespace Mvt = outshine::Test::Mvt;
  const std::vector<uint8_t> tile = Mvt::Tile({Mvt::Layer(
      "streets",
      {Mvt::Shape{Mvt::Geometry::Line,
                  {1024, 2048, 1536, 1792},
                  {Mvt::Says("kind", "residential"), Mvt::Counts("lanes", 2.0)}}},
      kExtent)});
  Note("bytes in the hand-encoded vector tile", (double)tile.size(), "bytes");

  const int added = field.Accept(kTileX, kTileY, tile);
  Note("features the field took from it", (double)added, "features");
  Note("bad tiles it counted", (double)field.BadTiles(), "tiles");
  Note("layers it found missing in the tile", (double)field.MissingLayers(), "of 5");

  CHECK(added == 1 && field.Features().size() == 1,
        "**A VECTOR TILE BECOMES A FIELD WITHOUT A POOL BEHIND IT**: Accept is a pure function "
        "of the bytes, so the whole ground layer's entry point is reachable by a proof that "
        "fetches nothing -- which is why it had none until now (board:1806)");
  CHECK(field.MissingLayers() == 4,
        "and the four layers this tile does not carry are counted as absent rather than as "
        "undecodable -- a tile that simply has no water is not a broken tile");
  CHECK(field.BadTiles() == 0, "and nothing in it failed to decode");

  const OsmField::Feature &one = field.Features().front();
  Note("the layer column it landed in", (double)one.Layer, "");
  Note("its geometry type", (double)one.Type, "2 is a line");
  Note("rings it carries", (double)one.RingCount, "rings");
  CHECK((int)one.Layer == field.Layer(OsmLayer::Streets) && one.Type == 2 && one.RingCount == 1,
        "and it is a line in the streets column, carrying one ring");

  const OsmField::Ring &ring = field.Rings()[one.FirstRing];
  Note("points in that ring", (double)ring.Count, "points");
  CHECK(ring.Count == 2, "with the two points the geometry stream spells");

  CHECK(field.Str(one, "kind") == "residential",
        "**AND ITS TAGS ARE READABLE BY NAME**: the key and the string pool are interned on the "
        "way in and resolve back out, so a harvester asks for 'kind' rather than for column 0");
  CHECK_NEAR(field.Num(one, "lanes", -1.0), 2.0, 1.0e-12, "lanes",
             "and a numeric tag comes back as a number rather than as text");
  CHECK_NEAR(field.Num(one, "not_a_tag", -7.5), -7.5, 1.0e-12, "",
             "and a tag the feature does not carry answers with the caller's own default, so "
             "an absent tag is not a zero");

  // the two points must land inside the tile they were decoded from, and the tile's box comes
  // from the web-mercator formulas rather than from the code under test.
  const Corner topLeft = CornerOf(kZoom, kTileX, kTileY);
  const Corner bottomRight = CornerOf(kZoom, kTileX + 1, kTileY + 1);
  Note("the tile's north edge", topLeft.LatDeg, "deg");
  Note("its south edge", bottomRight.LatDeg, "deg");
  Note("its west edge", topLeft.LonDeg, "deg");
  Note("its east edge", bottomRight.LonDeg, "deg");
  bool inside = true;
  for (uint32_t at = 0; at < ring.Count; ++at) {
    const double latDeg = field.Points()[2 * ((size_t)ring.First + at)];
    const double lonDeg = field.Points()[2 * ((size_t)ring.First + at) + 1];
    std::printf("NOTE point %u at %.6f, %.6f\n", at, latDeg, lonDeg);
    inside = inside && latDeg <= topLeft.LatDeg && latDeg >= bottomRight.LatDeg &&
             lonDeg >= topLeft.LonDeg && lonDeg <= bottomRight.LonDeg;
  }
  CHECK(inside,
        "**AND EVERY POINT LANDS INSIDE THE TILE IT WAS DECODED FROM**: the tile's own box is "
        "computed here from the web-mercator formulas rather than from the code under test, so "
        "this is the projection being checked and not the decoder agreeing with itself");
  CHECK_NEAR(field.Points()[2 * (size_t)ring.First + 1], topLeft.LonDeg +
                 (bottomRight.LonDeg - topLeft.LonDeg) * 1024.0 / (double)kExtent, 1.0e-9, "deg",
             "and the first point sits exactly a quarter of the tile in, because longitude is "
             "linear in the tile fraction and 1024 of 4096 is a quarter");

  Note("the field's heap", (double)field.HeapBytes(), "bytes");
  Note("is the tile settled", field.Settled(kTileX, kTileY) ? 1.0 : 0.0, "");
  Note("its index", (double)field.TileIndex(kTileX, kTileY), "");
  CHECK(field.HeapBytes() > 0 && field.Settled(kTileX, kTileY) &&
            field.TileIndex(kTileX, kTileY) == 0,
        "**AND A TILE THE FIELD ACCEPTED IS BOTH INDEXED AND SETTLED**: the two doors that ask "
        "whether a tile is in the field read two containers, and before board:1807 only one of "
        "them was written by the accept path -- a tile with 668 bytes of features in it "
        "answered no to the other (board:1807)");
  CHECK(field.OfTile(field.TileIndex(kTileX, kTileY)).size() == 1,
        "and asking the field for that tile's features returns the one it took");
  CHECK(field.TileIndex(kTileX + 1, kTileY) < 0,
        "while a tile it never accepted has no index rather than index zero");

  Covers("I.98 a vector tile becomes a field the ground layer can read: the decode is a pure "
         "function of the bytes with no pool behind it, tags resolve by name, an absent tag "
         "answers with the caller's default, and every point lands inside the tile it came "
         "from (board:1806)");
  return Report();
}
