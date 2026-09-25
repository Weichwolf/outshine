#include "src/generators/road/Corridors.h"
#include "src/generators/road/ProfiledRoadMesher.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool SameGeometry(const outshine::Geometry &left, const outshine::Geometry &right) {
  if (left.parts() != right.parts() || left.surfaces() != right.surfaces()) { return false; }
  for (int surface = 0; surface < left.surfaces(); ++surface) {
    if (left.surfaceNameOf(surface) != right.surfaceNameOf(surface) ||
        !(left.surfaceAt(outshine::MaterialInstance(surface)) ==
          right.surfaceAt(outshine::MaterialInstance(surface)))) {
      return false;
    }
  }
  for (int part = 0; part < left.parts(); ++part) {
    if (left.nameOf(part) != right.nameOf(part) ||
        left.materialOf(part) != right.materialOf(part) ||
        !std::ranges::equal(left.positionsOf(part), right.positionsOf(part)) ||
        !std::ranges::equal(left.normalsOf(part), right.normalsOf(part)) ||
        !std::ranges::equal(left.coloursOf(part), right.coloursOf(part)) ||
        !std::ranges::equal(left.trianglesOf(part), right.trianglesOf(part))) {
      return false;
    }
  }
  return true;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  constexpr LongitudeLatitude origin{.LongitudeDeg = 8.0, .LatitudeDeg = 49.0};
  const std::array<std::string, 5> layers{Ground::OsmLayerName(Ground::OsmLayer::Buildings),
                                          Ground::OsmLayerName(Ground::OsmLayer::WaterPolygons),
                                          Ground::OsmLayerName(Ground::OsmLayer::WaterLines),
                                          Ground::OsmLayerName(Ground::OsmLayer::Streets),
                                          Ground::OsmLayerName(Ground::OsmLayer::StreetPolygons)};
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "road fixture loads production cover rules");
  const int asphaltRow = materials.Find("asphalt");
  CHECK(asphaltRow >= 0, "road fixture has an explicit asphalt material");
  if (asphaltRow < 0) { return Report(); }
  const Vec3f asphalt = materials.At(static_cast<size_t>(asphaltRow)).Albedo;
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty,
                    .Field = [](EastNorth) -> std::optional<double> { return 0.0; }};
  const std::shared_ptr<const ClassStructure> noClasses;
  const Generators::ProfiledRoadMesher mesher;
  const Generators::Corridors corridors(mesher);

  Vec3f trackColour;
  Vec3f residentialColour;
  Vec3f mixedColour;
  constexpr std::array<std::array<std::string_view, 3>, 4> kKinds{{
      {{"track", "track", "track"}},
      {{"residential", "residential", "residential"}},
      {{"track", "track", "residential"}},
      {{"track", "track", "residential"}},
  }};
  for (size_t variant = 0; variant < kKinds.size(); ++variant) {
    Ground::OsmField vectors(14, layers);
    std::array<Ground::OsmField::Declared, 3> declared{
        {{.Layer = "streets",
          .Key = "kind",
          .Value = std::string(kKinds[variant][0]),
          .LatLon = {48.9998, 7.9990, 49.0, 8.0, 49.0002, 8.0010}},
         {.Layer = "streets",
          .Key = "kind",
          .Value = std::string(kKinds[variant][1]),
          .LatLon = {48.9990, 8.0, 49.0, 8.0, 49.0010, 8.0}},
         {.Layer = "streets",
          .Key = "kind",
          .Value = std::string(kKinds[variant][2]),
          .LatLon = {49.0, 8.0, 49.0007, 8.0008}}}};
    if (variant == 3) { std::swap(declared[0], declared[2]); }
    const auto tile = Ground::OsmField::Locate(origin, vectors.Zoom());
    CHECK(tile.has_value(), "fixture origin has a vector tile");
    if (!tile) { return Report(); }
    vectors.Declare(declared, *tile);
    Ground::StreetField ways;
    while (!ways.Ingested(vectors)) { (void)ways.Ingest(vectors, vegetation); }
    CHECK(ways.Ways().size() == declared.size(), "every road reaches the native map");
    if (ways.Ways().size() != declared.size()) { return Report(); }
    const int32_t row = ways.Ways().front().CoverRow;
    CHECK(row >= 0 && static_cast<size_t>(row) < vegetation.TemplateCount(),
          "OSM kind resolves to a production cover row");
    if (row < 0 || static_cast<size_t>(row) >= vegetation.TemplateCount()) { return Report(); }
    const Vec4f cover = vegetation.Rows()[static_cast<size_t>(row)].Ground;
    const Vec3f expected = {{cover[0], cover[1], cover[2]}};
    const Generators::Corridors::Site site{.Vectors = &vectors,
                                           .Ways = ways,
                                           .Materials = materials,
                                           .Vegetation = vegetation,
                                           .Standing = frame,
                                           .Draped = drape,
                                           .Classes = noClasses,
                                           .EyeLatDeg = origin.LatitudeDeg,
                                           .EyeLonDeg = origin.LongitudeDeg,
                                           .FocalPx = 800.0};
    Geometry mesh;
    std::vector<EarthworkStamp> earthworks;
    std::vector<DiagnosticSample> measures;
    CHECK(corridors.Lay(site, mesh, &earthworks, &measures) && mesh.wellFormed(),
          "the joined roads publish valid geometry");
    Geometry sliced;
    std::vector<EarthworkStamp> slicedEarthworks;
    std::vector<DiagnosticSample> slicedMeasures;
    std::unique_ptr<Generators::Corridors::Job> job = Generators::Corridors::Begin(site);
    bool complete = false;
    for (size_t step = 0; step < 1024 && !complete; ++step) {
      const auto advanced =
          corridors.Advance(*job, site, 1, 1, sliced, &slicedEarthworks, &slicedMeasures);
      if (!advanced) { break; }
      complete = *advanced;
    }
    CHECK(complete && SameGeometry(mesh, sliced) && earthworks == slicedEarthworks,
          "paced road construction preserves junction covers and terrain geometry exactly");
    CHECK(mesh.parts() == 1, "joined roads publish one colored corridor part");
    if (mesh.parts() != 1) { return Report(); }
    const std::span<const float> colors = mesh.coloursOf(0);
    CHECK(!colors.empty() && colors.size() % 4u == 0u, "the corridor has vertex colors");
    size_t mismatched = 0;
    size_t asphaltVertices = 0;
    std::vector<Vec3f> palette;
    for (size_t at = 0; at + 3 < colors.size(); at += 4) {
      const Vec3f actual = {{colors[at], colors[at + 1], colors[at + 2]}};
      mismatched += actual != expected ? 1u : 0u;
      asphaltVertices += actual == asphalt ? 1u : 0u;
      if (std::find(palette.begin(), palette.end(), actual) == palette.end()) {
        palette.push_back(actual);
      }
    }
    if (variant < 2) {
      CHECK(mismatched == 0, "joined way and connector colors follow the same OSM cover");
    }
    if (variant == 0) {
      trackColour = expected;
      CHECK(asphaltVertices == 0, "unsealed track connectors contain no asphalt-colored vertices");
    } else if (variant == 1) {
      residentialColour = expected;
    } else {
      const auto blend = std::find_if(palette.begin(), palette.end(), [&](const Vec3f &colour) {
        return colour != trackColour && colour != residentialColour;
      });
      CHECK(blend != palette.end(), "mixed junction colors derive from both incident covers");
      if (blend == palette.end()) { return Report(); }
      if (variant == 2) {
        mixedColour = *blend;
      } else {
        for (size_t axis = 0; axis < 3; ++axis) {
          CHECK(std::fabs((*blend)[axis] - mixedColour[axis]) < 1.0e-6f,
                "source-way order preserves mixed junction color");
        }
      }
    }
  }
  CHECK(trackColour != residentialColour, "different OSM way kinds change the road cover");
  return Report();
}
