#include "Check.h"
#include "StructureBuildQueue.h"
#include <array>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"building"};
  Generators::Osm::OsmField vectors(14, layers);
  const std::array<Generators::Osm::OsmField::Declared, 0> features;
  vectors.Declare(features, {.X = 18, .Y = 27});
  Generators::Osm::BuildingField field;
  field.AnchorAt({{0, 0, 0}});
  field.SeenWith({.FocalPx = 720});
  field.TilesSpan(1000);
  const StructureBuildQueue::BakeRevision revision{.Vectors = vectors.Generation(),
                                                   .Projection = field.Projection(),
                                                   .TileSpanM = field.TileSpanM(),
                                                   .Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47},
                                                   .RequestedDetail = LevelOfDetail::Fine};
  using Heights = StructureBuildQueue::HeightRequirement;
  CHECK(revision.Matches(vectors,
                         field,
                         {.LongitudeDeg = 9.0001, .LatitudeDeg = 47},
                         {},
                         Heights::AllowFallback,
                         LevelOfDetail::Fine),
        "the guarded Fine cell remains reusable for nearby movement");
  CHECK(!revision.Matches(vectors,
                          field,
                          {.LongitudeDeg = 9.01, .LatitudeDeg = 47},
                          {},
                          Heights::AllowFallback,
                          LevelOfDetail::Fine),
        "a remote eye cannot reuse projected facade relief");
  field.SeenWith({.FocalPx = 1080});
  CHECK(!revision.OwnsReservation(vectors, field, {.LongitudeDeg = 9, .LatitudeDeg = 47}, {}),
        "a changed pixel scale invalidates Fine relief ownership");
  field.SeenWith({.FocalPx = 720, .AllowedErrorPx = 0.25});
  CHECK(!revision.OwnsReservation(vectors, field, {.LongitudeDeg = 9, .LatitudeDeg = 47}, {}),
        "a changed pixel allowance invalidates Fine relief ownership");
  return Report();
}
