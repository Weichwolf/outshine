#include "StructureBakes.h"
#include "Check.h"
#include <array>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"building"};
  Ground::OsmField vectors(14, layers);
  const std::array<Ground::OsmField::Declared, 0> noFeatures;
  vectors.Declare(noFeatures, {.X = 18, .Y = 27});
  Ground::BuildingField footprints;
  footprints.AnchorAt({{0, 0, 0}});
  footprints.SeenWith(720.0);
  footprints.TilesSpan(2400.0);
  const StructureBakes::BakeRevision revision{.Vectors = vectors.Generation(),
                                              .FocalPx = footprints.FocalPx(),
                                              .TileSpanM = footprints.TileSpanM()};
  CHECK(revision.Matches(vectors, footprints), "posted bake inputs still match");
  footprints.SeenWith(1080.0);
  CHECK(!revision.Matches(vectors, footprints), "changed focal scale makes a bake stale");
  footprints.SeenWith(720.0);
  footprints.ResetDerived();
  CHECK(revision.Matches(vectors, footprints),
        "accepted footprints do not invalidate sibling bakes");
  const StructureBakes::BakeRevision vectorRevision{.Vectors = vectors.Generation(),
                                                    .FocalPx = footprints.FocalPx(),
                                                    .TileSpanM = footprints.TileSpanM()};
  CHECK(vectorRevision.Matches(vectors, footprints), "declared vector source still matches");
  vectors.Declare(noFeatures, {.X = 19, .Y = 27});
  CHECK(!vectorRevision.Matches(vectors, footprints),
        "changed vector source identity makes a bake stale");
  return Report();
}
