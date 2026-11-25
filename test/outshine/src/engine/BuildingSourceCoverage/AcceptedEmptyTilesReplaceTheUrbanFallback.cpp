#include "BuildingField.h"
#include "BuildingSourceCoverage.h"
#include "Check.h"
#include "GeometryPageResidency.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Field = Generators::Osm::BuildingField;
  const Render::GeometryPageResidency pieces;
  BuildingSourceCoverage coverage;
  Field footprints;
  footprints.AnchorAt({{1, 2, 3}});
  const Data::TileSourceIdentity source{.Kind = Data::DataKind::VectorMap,
                                        .Tile = {.Zoom = 14, .X = 4, .Y = 7},
                                        .SourceId = "osm",
                                        .Revision = "empty"};
  const Field::Baked empty;
  CHECK(coverage.Update(pieces, footprints) && coverage.Regions().empty(),
        "unfetched tiles retain the urban fallback");
  footprints.PreparesAcceptances(1);
  footprints.Take(7);
  footprints.CommitAcceptance(footprints.PrepareAcceptance(7, empty, {}, false, source), empty);
  CHECK(coverage.Update(pieces, footprints) && coverage.Regions().empty(),
        "unqualified tiles retain the urban fallback");
  footprints.ReplaceAcceptance(footprints.PrepareAcceptance(7, empty, {}, true, source), empty);
  CHECK(coverage.Update(pieces, footprints) && coverage.Regions().size() == 1 &&
            coverage.Regions().front() == Ground::TileBounds(source.Tile),
        "a confirmed empty tile replaces the fallback without a geometry upload");
  const auto former = coverage.Version();
  footprints.ResetDerived();
  CHECK(coverage.Update(pieces, footprints) && coverage.Regions().empty() &&
            coverage.Version() != former,
        "retiring a known empty source releases the geographic exclusion");
  return Report();
}
