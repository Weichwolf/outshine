#include "StructureBakes.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Ground::OsmField vectors(14, {});
  Ground::BuildingField footprints;
  footprints.AnchorAt({{0, 0, 0}});
  footprints.SeenWith(720.0);
  footprints.TilesSpan(2400.0);
  const StructureBakes::BakeRevision revision{.Vectors = vectors.Generation(),
                                              .Footprints = footprints.Revision(),
                                              .FocalPx = footprints.FocalPx(),
                                              .TileSpanM = footprints.TileSpanM()};
  CHECK(revision.Matches(vectors, footprints), "posted bake inputs still match");
  footprints.SeenWith(1080.0);
  CHECK(!revision.Matches(vectors, footprints), "changed focal scale makes a bake stale");
  footprints.SeenWith(720.0);
  footprints.ResetDerived();
  CHECK(!revision.Matches(vectors, footprints), "changed footprint revision makes a bake stale");
  return Report();
}
