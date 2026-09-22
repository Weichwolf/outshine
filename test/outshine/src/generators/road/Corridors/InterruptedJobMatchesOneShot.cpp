#include "src/generators/road/Corridors.h"
#include "src/generators/road/RoadMesh.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace outshine;

struct Product {
  Geometry Mesh;
  std::vector<Yields> Earthworks;
  std::vector<DiagnosticSample> Measures;
  bool Complete = false;
};

Product RunJob(const Generators::Corridors &corridors,
               const Generators::Corridors::Site &site,
               size_t lanesMost,
               size_t nodesMost) {
  Product product;
  std::unique_ptr<Generators::Corridors::Job> job = Generators::Corridors::Begin(site);
  constexpr size_t kSlicesMost = 1024;
  for (size_t slice = 0; slice < kSlicesMost; ++slice) {
    const auto advanced = corridors.Advance(
        *job, site, lanesMost, nodesMost, product.Mesh, &product.Earthworks, &product.Measures);
    if (!advanced) { return product; }
    if (*advanced) {
      product.Complete = true;
      return product;
    }
  }
  return product;
}

bool SameGeometry(const Geometry &left, const Geometry &right) {
  if (left.parts() != right.parts() || left.surfaces() != right.surfaces()) { return false; }
  for (int surface = 0; surface < left.surfaces(); ++surface) {
    if (left.surfaceNameOf(surface) != right.surfaceNameOf(surface) ||
        !(left.surfaceAt(MaterialInstance(surface)) ==
          right.surfaceAt(MaterialInstance(surface)))) {
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

bool SameSemanticMeasures(std::span<const DiagnosticSample> left,
                          std::span<const DiagnosticSample> right) {
  size_t candidate = 0;
  for (const DiagnosticSample &expected : left) {
    while (candidate < right.size() && right[candidate].Name != expected.Name) { ++candidate; }
    if (candidate == right.size() || right[candidate].Unit != expected.Unit ||
        (expected.Unit != "ms" && right[candidate].Value != expected.Value)) {
      return false;
    }
    ++candidate;
  }
  return true;
}

}

int main() {
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8.0, .LatitudeDeg = 49.0};
  const std::array<std::string, 5> layers{Ground::OsmLayerName(Ground::OsmLayer::Buildings),
                                          Ground::OsmLayerName(Ground::OsmLayer::WaterPolygons),
                                          Ground::OsmLayerName(Ground::OsmLayer::WaterLines),
                                          Ground::OsmLayerName(Ground::OsmLayer::Streets),
                                          Ground::OsmLayerName(Ground::OsmLayer::StreetPolygons)};
  Ground::OsmField vectors(14, layers);
  const std::array<Ground::OsmField::Declared, 3> declared{
      {{.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .LatLon = {48.9998, 7.9990, 49.0, 8.0, 49.0002, 8.0010}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .Bridge = true,
        .Level = 1,
        .LatLon = {48.9990, 8.0, 49.0, 8.0, 49.0010, 8.0}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .LatLon = {49.0, 8.0, 49.0007, 8.0008}}}};
  const auto tile = Ground::OsmField::Locate(origin, vectors.Zoom());
  CHECK(tile.has_value(), "fixture origin has a vector tile");
  if (!tile) { return Report(); }
  vectors.Declare(declared, *tile);

  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "road fixture loads the production material rules");
  Ground::StreetField ways;
  while (!ways.Ingested(vectors)) { (void)ways.Ingest(vectors, vegetation); }
  CHECK(ways.Ways().size() == declared.size(), "every declared road reaches the native map");

  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty,
                    .Field = [](EastNorth) -> std::optional<double> { return 0.0; }};
  const std::shared_ptr<const ClassStructure> noClasses;
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
  const Generators::RoadMesh mesher;
  const Generators::Corridors corridors(mesher);
  Product oneShot;
  oneShot.Complete = corridors.Lay(site, oneShot.Mesh, &oneShot.Earthworks, &oneShot.Measures);
  CHECK(oneShot.Complete && oneShot.Mesh.wellFormed() && !oneShot.Earthworks.empty(),
        "one-shot road construction produces complete geometry and earthworks");

  for (const auto [lanesMost, nodesMost] :
       std::array<std::pair<size_t, size_t>, 2>{{{1, 1}, {2, 3}}}) {
    const Product sliced = RunJob(corridors, site, lanesMost, nodesMost);
    CHECK(sliced.Complete && sliced.Mesh.wellFormed(),
          "interrupted road construction reaches a complete native product");
    CHECK(SameGeometry(oneShot.Mesh, sliced.Mesh),
          "interruption preserves exact road vertices, attributes, indices and materials");
    CHECK(oneShot.Earthworks == sliced.Earthworks,
          "interruption preserves exact ordered terrain earthworks");
    CHECK(SameSemanticMeasures(oneShot.Measures, sliced.Measures),
          "interruption preserves ordered semantic diagnostics independently of timing");
  }
  std::unique_ptr<Generators::Corridors::Job> stale = Generators::Corridors::Begin(site);
  auto changed = declared;
  changed.front().LatLon.front() += 0.0001;
  vectors.Declare(changed, *tile);
  Geometry staleMesh;
  std::vector<Yields> staleEarthworks;
  std::vector<DiagnosticSample> staleMeasures;
  CHECK(!corridors.Advance(*stale, site, 1, 1, staleMesh, &staleEarthworks, &staleMeasures) &&
            staleMesh.parts() == 0 && staleEarthworks.empty() && staleMeasures.empty(),
        "a changed vector revision rejects stale work before publishing any product");
  return Report();
}
