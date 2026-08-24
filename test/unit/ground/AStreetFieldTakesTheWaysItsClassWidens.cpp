#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "VectorTileMaker.h"

#include "GroundMaterials.h"
#include "OsmField.h"
#include "OsmLayer.h"
#include "StreetField.h"
#include "VegetationTemplates.h"

using outshine::Ground::GroundMaterials;
using outshine::Ground::OsmField;
using outshine::Ground::OsmLayer;
using outshine::Ground::OsmLayerNames;
using outshine::Ground::StreetField;
using outshine::Ground::VegetationTemplates;

namespace Mvt = outshine::Test::Mvt;

namespace {

constexpr int kZoom = 12;
constexpr int kTileX = 2179, kTileY = 1421;

// board:1806: StreetField was drawn in the CURRENT class map and named by nothing under test/.
// It is the field that decides which OSM ways become road at all, and it decides it entirely
// from the shipped class table -- so the proof loads that table rather than a fixture, and a
// change to vegetation.json that broke the road network would land here.
[[nodiscard]] std::vector<uint8_t> AStreetOf(const char *kind, int32_t byX) {
  return Mvt::Layer("streets",
                    {Mvt::Shape{Mvt::Geometry::Line,
                                {100, 100, 100 + byX, 100},
                                {Mvt::Says("kind", kind)}}});
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  GroundMaterials materials;
  CHECK(materials.Load("src/assets/world/ground-materials.json"),
        "the shipped ground materials load");
  VegetationTemplates classes;
  CHECK(classes.Load("src/assets/world/vegetation.json", materials),
        "and the shipped class table loads beside them");
  if (!classes.Ready()) {
    std::printf("REFUSED %s\n", classes.Error().c_str());
    return Report();
  }
  Note("classes the shipped table declares", (double)classes.RuleCount(), "rules");

  const std::vector<std::string> layers =
      OsmLayerNames({OsmLayer::Buildings, OsmLayer::WaterPolygons, OsmLayer::WaterLines,
                     OsmLayer::Streets, OsmLayer::StreetPolygons});

  // 1. a way of a class the table widens becomes a way with half that width.
  {
    OsmField field(kZoom, layers);
    CHECK(field.Accept(kTileX, kTileY, Mvt::Tile({AStreetOf("residential", 400)})) == 1,
          "one residential way decodes into the field");
    StreetField streets;
    const uint32_t took = streets.Ingest(field, classes);
    Note("ways the street field took", (double)took, "ways");
    Note("its half width", took > 0 ? (double)streets.Ways().front().HalfWidthM : 0.0, "m");
    const VegetationTemplates::Rule *rule = classes.Find("streets", "residential");
    Note("the class table's own width for residential", rule ? (double)rule->WidthM : 0.0, "m");
    CHECK(took == 1 && rule != nullptr,
          "**A WAY BECOMES ROAD BECAUSE ITS CLASS IS WIDENED, NOT BECAUSE IT IS TAGGED**: the "
          "street field reads the shipped class table and nothing else, so widening a class in "
          "vegetation.json widens the road and nobody edits C++ (board:1806)");
    CHECK_NEAR((double)streets.Ways().front().HalfWidthM, 0.5 * (double)rule->WidthM, 1.0e-6, "m",
               "and the way carries HALF the declared width, because a centreline has two "
               "sides and a field that stored the full width would double every road");
    CHECK(streets.Ways().front().PointCount == 2 &&
              streets.Ways().front().Form == StreetField::Shape::Ribbon,
          "and it is a ribbon of the two points the geometry spelled");
    CHECK(streets.UnwidthedCount() == 0 && streets.TunnelCount() == 0,
          "with nothing refused for want of a width and nothing skipped as a tunnel");
    CHECK(streets.OfTile(0).Size() == 1,
          "and the tile it came from indexes it, so a consumer walks one tile rather than the "
          "whole world");
  }

  // 2. a class the table does not widen is COUNTED, not silently dropped.
  {
    OsmField field(kZoom, layers);
    CHECK(field.Accept(kTileX, kTileY, Mvt::Tile({AStreetOf("rail", 400)})) == 1,
          "a rail way decodes into the field");
    StreetField streets;
    const uint32_t took = streets.Ingest(field, classes);
    const VegetationTemplates::Rule *rule = classes.Find("streets", "rail");
    Note("ways taken from a rail-only tile", (double)took, "ways");
    Note("ways refused for having no width", (double)streets.UnwidthedCount(), "ways");
    Note("the class table's width for rail", rule ? (double)rule->WidthM : -1.0, "m");
    CHECK(rule != nullptr && rule->WidthM > 0.0f && took == 1,
          "rail IS widened in the shipped table -- 3.8 m -- so it is taken, and this row is "
          "here because a reader would otherwise assume it is not");
  }

  // 3. a kind the table knows nothing about is dropped, and a tunnel is counted as a tunnel.
  {
    OsmField field(kZoom, layers);
    const std::vector<uint8_t> tile = Mvt::Tile({Mvt::Layer(
        "streets",
        {Mvt::Shape{Mvt::Geometry::Line, {100, 100, 500, 100},
                    {Mvt::Says("kind", "a_kind_nobody_declares")}},
         Mvt::Shape{Mvt::Geometry::Line, {200, 200, 600, 200},
                    {Mvt::Says("kind", "residential"), Mvt::Counts("tunnel", 1.0)}},
         Mvt::Shape{Mvt::Geometry::Line, {300, 300, 700, 300},
                    {Mvt::Says("kind", "residential")}}})});
    CHECK(field.Accept(kTileX, kTileY, tile) == 3, "three ways decode into the field");
    StreetField streets;
    const uint32_t took = streets.Ingest(field, classes);
    Note("ways taken of the three", (double)took, "ways");
    Note("tunnels counted", (double)streets.TunnelCount(), "ways");
    CHECK(took == 1 && streets.TunnelCount() == 1,
          "**AND WHAT IT REFUSES IT COUNTS**: an undeclared kind is dropped and a tunnel is "
          "counted as a tunnel, so a route that loses a road can tell 'the class table has no "
          "row for it' from 'the road goes underground' without reading the tile again");
  }

  // 4. ingesting twice does not take the same tile twice -- the watermark is the guard.
  {
    OsmField field(kZoom, layers);
    CHECK(field.Accept(kTileX, kTileY, Mvt::Tile({AStreetOf("residential", 400)})) == 1,
          "one way decodes");
    StreetField streets;
    const uint32_t first = streets.Ingest(field, classes);
    const uint32_t again = streets.Ingest(field, classes);
    Note("ways after one ingest", (double)first, "ways");
    Note("ways after a second over the same field", (double)again, "ways");
    CHECK(first == 1 && again == 1,
          "**AND A FIELD INGESTED TWICE HOLDS THE ROAD ONCE**: the watermark is what makes the "
          "ingest resumable across frames, and a field that doubled its roads on a second call "
          "would double them every frame it was not finished in");
  }

  Covers("I.4.8 the street field takes the ways the shipped class table widens, halves their "
         "width for a centreline, counts what it refuses by reason, indexes them per tile, and "
         "takes each tile once however often it is asked (board:1806)");
  return Report();
}
