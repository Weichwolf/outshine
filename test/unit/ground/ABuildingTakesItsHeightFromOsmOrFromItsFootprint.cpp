#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "VectorTileMaker.h"

#include "BuildingField.h"
#include "GroundQuery.h"
#include "OsmField.h"
#include "OsmLayer.h"

using outshine::GroundQuery;
using outshine::GroundSample;
using outshine::Ground::BuildingField;
using outshine::Ground::OsmField;
using outshine::Ground::OsmLayer;
using outshine::Ground::OsmLayerNames;
using outshine::WayLine;

namespace Mvt = outshine::Test::Mvt;

namespace {

constexpr int kZoom = 12;
constexpr int kTileX = 2179, kTileY = 1421;

// board:1806: BuildingField was drawn in the CURRENT class map and named by nothing under
// test/. Its door took a GroundStream; narrowed to GroundQuery it is provable with a dozen
// lines of synthetic ground. Its one real decision is where a building's height comes from:
// OSM says it, or the footprint has to imply it.
class Level final : public GroundQuery {
public:
  explicit Level(double aslM) : AslM_(aslM) {}
  [[nodiscard]] GroundSample At(double lat, double lon) const override {
    (void)lat;
    (void)lon;
    return GroundSample::At(AslM_);
  }
  [[nodiscard]] double PostM(double latDeg) const override {
    (void)latDeg;
    return 30.0;
  }

private:
  double AslM_;
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

[[nodiscard]] std::vector<uint8_t> ABlockOf(int32_t side, const std::vector<Mvt::Tag> &tags) {
  return Mvt::Tile({Mvt::Layer(
      "buildings", {Mvt::Shape{Mvt::Geometry::Polygon,
                               {1000, 1000, 1000 + side, 1000, 1000 + side, 1000 + side,
                                1000, 1000 + side},
                               tags}})});
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const double anchor[3] = {4177000.0, 852000.0, 4728000.0};
  const std::vector<WayLine> noStreets;

  // 1. a building that declares its height gets the height it declares, and says so.
  {
    OsmField field(kZoom, Layers());
    CHECK(field.Accept(kTileX, kTileY, ABlockOf(200, {Mvt::Counts("height", 27.5)})) == 1,
          "a block declaring 27.5 m decodes into the field");
    Level flat(500.0);
    BuildingField blocks;
    blocks.AnchorAt(anchor);
    blocks.Build(flat, field, outshine::Span<const WayLine>(noStreets.data(), noStreets.size()));
    Note("footprints the field took", (double)blocks.Footprints().size(), "footprints");
    Note("heights it read from OSM", (double)blocks.OsmHeights(), "");
    Note("heights it had to imply", (double)blocks.DefaultHeights(), "");
    CHECK(blocks.Footprints().size() == 1, "and becomes one footprint");
    const BuildingField::Footprint &one = blocks.Footprints().front();
    Note("the height it carries", (double)one.HeightM, "m");
    Note("the base it sits on", (double)one.BaseM, "m");
    CHECK_NEAR((double)one.HeightM, 27.5, 1.0e-4, "m",
               "**A BUILDING THAT DECLARES ITS HEIGHT GETS THE HEIGHT IT DECLARES**: the OSM "
               "tag wins over any rule the field could apply, because a measured building is "
               "better data than an implied one (board:1806)");
    CHECK(one.Source == BuildingField::HeightSource::Osm && blocks.OsmHeights() == 1 &&
              blocks.DefaultHeights() == 0,
          "and it RECORDS which of the two it was, so a picture full of guessed heights can be "
          "told from one full of surveyed ones without measuring the buildings");
    CHECK_NEAR((double)one.BaseM, 500.0, 1.0e-3, "m",
               "and it sits on the ground the query answered, not at sea level");
  }

  // 2. a building with no height is given one from its footprint AND its relation to a street.
  //    Measured, the street is what decides: a plot standing on one carries two to five storeys
  //    where the same plot in a field carries one or two. My first version of this arm asserted
  //    that a bigger plan area carries more storeys, and the rule says no such thing -- a
  //    120-unit block and a 900-unit block both came out at 9.000 m, because without a street
  //    both fall into the same band.
  {
    const auto ImpliedHeight = [&](bool beside, double &standBack) {
      OsmField field(kZoom, Layers());
      if (field.Accept(kTileX, kTileY, ABlockOf(12, {Mvt::Says("kind", "yes")})) != 1) {
        return 0.0;
      }
      const OsmField::Ring &ring = field.Rings().front();
      const double refLat = field.Points()[(size_t)ring.First * 2];
      const double refLon = field.Points()[(size_t)ring.First * 2 + 1];
      const double asideDeg = 6.0 / 111000.0;
      const std::vector<double> kerb = {refLat - asideDeg, refLon - 0.002,
                                        refLat - asideDeg, refLon + 0.002};
      std::vector<WayLine> streets;
      if (beside) {
        WayLine one;
        one.LatLon = outshine::Span<const double>(kerb.data(), kerb.size());
        one.HalfWidthM = 3.5;
        one.MinLat = refLat - asideDeg;
        one.MaxLat = refLat - asideDeg;
        one.MinLon = refLon - 0.002;
        one.MaxLon = refLon + 0.002;
        streets.push_back(one);
      }
      Level flat(500.0);
      BuildingField blocks;
      blocks.AnchorAt(anchor);
      blocks.Build(flat, field, outshine::Span<const WayLine>(streets.data(), streets.size()));
      if (blocks.Footprints().empty()) { return 0.0; }
      standBack = blocks.Footprints().front().Street.Known ? 1.0 : 0.0;
      return (double)blocks.Footprints().front().HeightM;
    };

    double frontedKnown = 0.0, aloneKnown = 0.0;
    const double alone = ImpliedHeight(false, aloneKnown);
    const double fronted = ImpliedHeight(true, frontedKnown);
    Note("the implied height of a plot standing alone", alone, "m");
    Note("the implied height of the same plot on a street", fronted, "m");
    Note("did the alone one find a frontage", aloneKnown, "");
    Note("did the fronted one", frontedKnown, "");

    CHECK(alone > 0.0 && fronted > 0.0,
          "both untagged blocks are given a standing height rather than a flat roof at ground "
          "level");
    CHECK(frontedKnown > 0.0 && aloneKnown == 0.0,
          "and the field knows which of them has a street in front of it, which is the input "
          "the height rule turns on");
    CHECK(fronted > alone,
          "**AND AN IMPLIED HEIGHT IS IMPLIED BY THE STREET, NOT BY THE PLAN AREA**: the same "
          "footprint standing on a street carries more storeys than one standing in a field, "
          "because that is what towns do -- and the guess is therefore a function of the "
          "building's PLACE rather than one constant wearing a rule's clothes (board:1806)");
    CHECK(std::fmod(alone - 3.2, 2.9) < 1.0e-4 || std::fmod(alone - 3.2, 2.9) > 2.9 - 1.0e-4,
          "and every implied height is a whole number of storeys plus one roof allowance, so "
          "the number is a building rather than a scalar somebody liked");
  }

  // 3. ground nobody has is not a building at a guessed base.
  {
    OsmField field(kZoom, Layers());
    CHECK(field.Accept(kTileX, kTileY, ABlockOf(200, {Mvt::Counts("height", 27.5)})) == 1,
          "the same block decodes");
    Nowhere nothing;
    BuildingField blocks;
    blocks.AnchorAt(anchor);
    blocks.Build(nothing, field,
                 outshine::Span<const WayLine>(noStreets.data(), noStreets.size()));
    Note("footprints taken where the ground answers nothing", (double)blocks.Footprints().size(),
         "footprints");
    CHECK(blocks.Footprints().empty(),
          "**AND A BUILDING OVER GROUND NOBODY HAS IS NOT PLACED**: a footprint whose base does "
          "not resolve produces nothing, rather than a tower floating at sea level -- an absent "
          "height is absent, never a zero");
  }

  Covers("I.4.10 a building takes its height from OSM where OSM has one and from its own "
         "footprint where it does not, records which of the two it was, sits on the ground the "
         "query answers, and is not placed at all where that ground is absent (board:1806)");
  return Report();
}
