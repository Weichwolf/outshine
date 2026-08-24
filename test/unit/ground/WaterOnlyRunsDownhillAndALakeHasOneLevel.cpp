#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "VectorTileMaker.h"

#include "GroundMaterials.h"
#include "GroundQuery.h"
#include "OsmField.h"
#include "OsmLayer.h"
#include "VegetationTemplates.h"
#include "WaterField.h"

using outshine::GroundQuery;
using outshine::GroundSample;
using outshine::Ground::GroundMaterials;
using outshine::Ground::OsmField;
using outshine::Ground::OsmLayer;
using outshine::Ground::OsmLayerNames;
using outshine::Ground::VegetationTemplates;
using outshine::Ground::WaterField;

namespace Mvt = outshine::Test::Mvt;

namespace {

constexpr int kZoom = 12;
constexpr int kTileX = 2179, kTileY = 1421;

// board:1806: WaterField was drawn in the CURRENT class map and named by nothing under test/.
// Its door took a GroundStream -- a TilePool, threads, a content store -- and it asks the
// ground exactly one question. Narrowed to GroundQuery the way board:1624 narrowed LayCorridor,
// a synthetic ground is a dozen lines and the field's two real decisions become provable:
// a water COURSE runs downhill and a water SURFACE has one level.
class Hilly final : public GroundQuery {
public:
  // a ridge running east-west: height falls away from a crest, so a course crossing it climbs
  // and then descends, and the field has to decide which way the water goes.
  [[nodiscard]] GroundSample At(double lat, double lon) const override {
    (void)lat;
    ++Asked_;
    const double fromCrest = (lon - 11.54) * 1000.0;
    return GroundSample::At(500.0 - std::fabs(fromCrest) * 40.0);
  }
  [[nodiscard]] double PostM(double latDeg) const override {
    (void)latDeg;
    return 30.0;
  }
  [[nodiscard]] size_t Asked() const { return Asked_; }

private:
  mutable size_t Asked_ = 0;
};

class Nowhere final : public GroundQuery {
public:
  [[nodiscard]] GroundSample At(double lat, double lon) const override {
    (void)lat;
    (void)lon;
    return GroundSample::Missing();
  }
  [[nodiscard]] double PostM(double latDeg) const override {
    (void)latDeg;
    return 30.0;
  }
};

[[nodiscard]] std::vector<std::string> Layers() {
  return OsmLayerNames({OsmLayer::Buildings, OsmLayer::WaterPolygons, OsmLayer::WaterLines,
                        OsmLayer::Streets, OsmLayer::StreetPolygons});
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  GroundMaterials materials;
  VegetationTemplates classes;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            classes.Load("src/assets/world/vegetation.json", materials),
        "the shipped ground materials and class table load");
  if (!classes.Ready()) { return Report(); }

  const double anchor[3] = {4177000.0, 852000.0, 4728000.0};

  // 1. a river crossing a ridge: the ground under it rises and falls, and the water may not.
  {
    OsmField field(kZoom, Layers());
    std::vector<int32_t> along;
    for (int32_t at = 0; at < 9; ++at) { along.push_back(200 + at * 400); along.push_back(2048); }
    CHECK(field.Accept(kTileX, kTileY,
                       Mvt::Tile({Mvt::Layer("water_lines",
                                             {Mvt::Shape{Mvt::Geometry::Line, along,
                                                         {Mvt::Says("kind", "river")}}})})) == 1,
          "a nine-point river decodes into the field");

    Hilly hills;
    WaterField water;
    water.AnchorAt(anchor);
    water.Ingest(hills, field, classes);
    Note("courses the water field took", (double)water.Courses().size(), "courses");
    Note("ground queries it made", (double)hills.Asked(), "queries");
    CHECK(water.Courses().size() == 1, "and it becomes one water course");

    const WaterField::Course &course = water.Courses().front();
    double groundUp = 0.0, waterUp = 0.0;
    std::printf("NOTE station | ground asl | water level\n");
    for (uint32_t at = 0; at < course.PointCount; ++at) {
      const double latDeg = field.Points()[2 * ((size_t)course.FirstPoint + at)];
      const double lonDeg = field.Points()[2 * ((size_t)course.FirstPoint + at) + 1];
      double aslM = 0.0;
      (void)hills.At(latDeg, lonDeg).TryAslM(&aslM);
      const double level = (double)water.Levels()[course.FirstLevel + at];
      std::printf("NOTE   %2u    | %10.3f | %10.3f\n", at, aslM, level);
      if (at > 0) {
        double beforeAsl = 0.0;
        (void)hills.At(field.Points()[2 * ((size_t)course.FirstPoint + at - 1)],
                       field.Points()[2 * ((size_t)course.FirstPoint + at - 1) + 1])
            .TryAslM(&beforeAsl);
        groundUp = std::max(groundUp, aslM - beforeAsl);
        waterUp = std::max(waterUp,
                           level - (double)water.Levels()[course.FirstLevel + at - 1]);
      }
    }
    Note("the most the GROUND climbs between two stations", groundUp, "m");
    Note("the most the WATER climbs between them", waterUp, "m");
    CHECK(groundUp > 1.0,
          "the synthetic ridge really does climb under the river, so the next claim is not "
          "vacuous");
    CHECK(waterUp <= 0.0,
          "**WATER ONLY RUNS DOWNHILL**: the level is forced monotone from whichever end is "
          "higher, so a course crossing a ridge does not carry water uphill -- the terrain is "
          "sampled, and where it climbs, the surface is cut into it rather than following it "
          "(board:1806)");

    bool onOrUnder = true;
    for (uint32_t at = 0; at < course.PointCount; ++at) {
      double aslM = 0.0;
      (void)hills.At(field.Points()[2 * ((size_t)course.FirstPoint + at)],
                     field.Points()[2 * ((size_t)course.FirstPoint + at) + 1])
          .TryAslM(&aslM);
      onOrUnder = onOrUnder && (double)water.Levels()[course.FirstLevel + at] <= aslM + 1.0e-6;
    }
    CHECK(onOrUnder,
          "and nowhere does the water stand above the ground it runs over, which is what says "
          "the monotone pass cuts rather than fills");

    const VegetationTemplates::Rule *river = classes.Find("water_lines", "river");
    Note("the half width the course carries", (double)course.HalfWidthM, "m");
    CHECK(course.HalfWidthM > 0.0f &&
              (river == nullptr || river->WidthM <= 0.0f ||
               std::fabs((double)course.HalfWidthM - 0.5 * (double)river->WidthM) < 1.0e-6),
          "and its width is the class table's own, halved -- or one metre where the table "
          "declares none, so a course is never zero-wide");
  }

  // 2. a lake: one level for the whole ring, taken low so a shore is a shore.
  {
    OsmField field(kZoom, Layers());
    CHECK(field.Accept(kTileX, kTileY,
                       Mvt::Tile({Mvt::Layer(
                           "water_polygons",
                           {Mvt::Shape{Mvt::Geometry::Polygon,
                                       {1000, 1000, 3000, 1000, 3000, 3000, 1000, 3000},
                                       {Mvt::Says("kind", "water")}}})})) == 1,
          "a four-corner lake decodes into the field");
    Hilly hills;
    WaterField water;
    water.AnchorAt(anchor);
    water.Ingest(hills, field, classes);
    Note("surfaces the water field took", (double)water.Surfaces().size(), "surfaces");
    CHECK(water.Surfaces().size() == 1, "and it becomes one surface");

    const WaterField::Surface &lake = water.Surfaces().front();
    double lowest = 1.0e9, highest = -1.0e9;
    for (uint32_t at = 0; at < lake.PointCount; ++at) {
      double aslM = 0.0;
      (void)hills.At(field.Points()[2 * ((size_t)lake.FirstPoint + at)],
                     field.Points()[2 * ((size_t)lake.FirstPoint + at) + 1])
          .TryAslM(&aslM);
      lowest = std::min(lowest, aslM);
      highest = std::max(highest, aslM);
    }
    Note("the lowest ground on its shore", lowest, "m");
    Note("the highest", highest, "m");
    Note("the level the field gave the lake", (double)lake.LevelM, "m");
    Note("shore points that stand more than the tolerance above it",
         (double)water.OutlierCount(), "of 4");
    CHECK((double)lake.LevelM >= lowest - 1.0e-6 && (double)lake.LevelM <= highest + 1.0e-6,
          "**AND A LAKE HAS ONE LEVEL, TAKEN FROM ITS OWN SHORE**: a surface is flat by "
          "definition, so the field picks one height out of the ring's own ground rather than "
          "following it -- a lake that followed the terrain would be a hillside");
    CHECK_NEAR((double)lake.LevelM, lowest, 1.0e-6, "m",
               "and it takes it LOW -- the fifth percentile of the shore -- because a level "
               "above the shore floods the land around it, and a level below it merely leaves "
               "a bank");
    CHECK(water.OutlierCount() > 0,
          "and shore points standing more than five metres above that level are counted, so a "
          "polygon that is not really a lake can be told from one that is");
  }

  // 3. ground that answers nothing is counted, not guessed at.
  {
    OsmField field(kZoom, Layers());
    CHECK(field.Accept(kTileX, kTileY,
                       Mvt::Tile({Mvt::Layer(
                           "water_polygons",
                           {Mvt::Shape{Mvt::Geometry::Polygon,
                                       {1000, 1000, 3000, 1000, 3000, 3000, 1000, 3000},
                                       {Mvt::Says("kind", "water")}}})})) == 1,
          "the same lake decodes");
    Nowhere nothing;
    WaterField water;
    water.AnchorAt(anchor);
    water.Ingest(nothing, field, classes);
    Note("surfaces taken where the ground answers nothing", (double)water.Surfaces().size(),
         "surfaces");
    Note("rings refused for want of ground", (double)water.NoGroundCount(), "rings");
    CHECK(water.Surfaces().empty(),
          "**AND WATER OVER GROUND NOBODY HAS IS NOT INVENTED**: a ring whose ground does not "
          "resolve produces no surface at all, rather than a lake at sea level -- an absent "
          "height is absent, never a zero");
  }

  Covers("I.4.9 water only runs downhill and a lake has one level: a course's levels are "
         "forced monotone from its higher end and never stand above the ground, a surface "
         "takes the fifth percentile of its own shore, and a ring whose ground does not "
         "resolve produces nothing (board:1806)");
  return Report();
}
