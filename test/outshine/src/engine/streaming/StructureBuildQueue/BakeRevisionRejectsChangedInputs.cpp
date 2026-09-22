#include "StructureBuildQueue.h"
#include "Check.h"
#include <array>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(StructureBuildQueue::kCandidateWindow == 4,
        "structure admission keeps the measured four-candidate window across hardware sizes");
  const std::array<std::string, 1> layers{"building"};
  Ground::OsmField vectors(14, layers);
  const std::array<Ground::OsmField::Declared, 0> noFeatures;
  vectors.Declare(noFeatures, {.X = 18, .Y = 27});
  Ground::BuildingField footprints;
  CHECK(!footprints.Anchored(), "a default footprint field refuses bake admission");
  footprints.AnchorAt({{0, 0, 0}});
  CHECK(footprints.Anchored(), "an anchored field admits bake scheduling");
  footprints.SeenWith(720.0);
  footprints.TilesSpan(2400.0);
  const StructureBuildQueue::BakeRevision revision{.Vectors = vectors.Generation(),
                                                   .FocalPx = footprints.FocalPx(),
                                                   .TileSpanM = footprints.TileSpanM(),
                                                   .Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47}};
  CHECK(revision.Matches(vectors, footprints, {.LongitudeDeg = 9, .LatitudeDeg = 47}),
        "posted bake inputs still match");
  const StructureBuildQueue::BakeRevision fallbackRevision{
      .Vectors = vectors.Generation(),
      .FocalPx = footprints.FocalPx(),
      .TileSpanM = footprints.TileSpanM(),
      .Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47},
      .FallbackHeights = true};
  CHECK(fallbackRevision.Matches(vectors, footprints, {.LongitudeDeg = 9, .LatitudeDeg = 47}),
        "playable accepts fallback heights");
  CHECK(!fallbackRevision.Matches(vectors,
                                  footprints,
                                  {.LongitudeDeg = 9, .LatitudeDeg = 47},
                                  StructureBuildQueue::HeightRequirement::FineOnly),
        "refined rejects a queued fallback bake");
  CHECK(revision.Matches(vectors,
                         footprints,
                         {.LongitudeDeg = 9, .LatitudeDeg = 47},
                         StructureBuildQueue::HeightRequirement::FineOnly),
        "refined accepts a queued fine bake");
  footprints.SeenWith(1080.0);
  CHECK(!revision.Matches(vectors, footprints, {.LongitudeDeg = 9, .LatitudeDeg = 47}),
        "changed focal scale makes a bake stale");
  footprints.SeenWith(720.0);
  footprints.ResetDerived();
  CHECK(revision.Matches(vectors, footprints, {.LongitudeDeg = 9, .LatitudeDeg = 47}),
        "accepted footprints do not invalidate sibling bakes");
  CHECK(!revision.Matches(vectors, footprints, {.LongitudeDeg = 9.01, .LatitudeDeg = 47}),
        "a moved camera makes an unfinished bake stale");
  const StructureBuildQueue::BakeRevision vectorRevision{
      .Vectors = vectors.Generation(),
      .FocalPx = footprints.FocalPx(),
      .TileSpanM = footprints.TileSpanM(),
      .Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47}};
  CHECK(vectorRevision.Matches(vectors, footprints, {.LongitudeDeg = 9, .LatitudeDeg = 47}),
        "declared vector source still matches");
  vectors.Declare(noFeatures, {.X = 19, .Y = 27});
  CHECK(!vectorRevision.Matches(vectors, footprints, {.LongitudeDeg = 9, .LatitudeDeg = 47}),
        "changed vector source identity makes a bake stale");
  return Report();
}
