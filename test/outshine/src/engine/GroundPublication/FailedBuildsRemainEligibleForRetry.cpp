#include "GroundPublication.h"
#include "Check.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  GroundPublication publication;
  const GroundRevision initial{
      .Region = 17, .ResidentTiles = 4, .Classes = 2, .Footprints = 3, .Projection = {0, 1.0, 2.0}};
  CHECK(!publication.Current() && publication.NeedsRebuild(initial, false, false),
        "an unpublished world needs its first build even without tile arrivals");
  CHECK(publication.NeedsRebuild(initial, false, false) && !publication.Current(),
        "requesting a build that never publishes does not suppress retry");
  CHECK(publication.Publish(initial), "the first complete ground revision publishes");
  CHECK(!publication.NeedsRebuild(initial, true, false),
        "the successfully published revision needs no duplicate build");
  std::array<GroundRevision, 10> changed{
      initial, initial, initial, initial, initial, initial, initial, initial, initial, initial};
  ++changed[0].Region;
  ++changed[1].Classes;
  ++changed[2].Footprints;
  ++changed[3].StreetTiles;
  ++changed[4].WaterTiles;
  changed[5].Projection[0] = 1;
  changed[6].Projection[1] = 0.5;
  changed[7].Projection[2] = 4;
  ++changed[8].VectorGeneration;
  ++changed[9].TransportSourceGeneration;
  for (const GroundRevision &requested : changed) {
    CHECK(publication.NeedsRebuild(requested, false, false),
          "region, source snapshot, data and projection changes rebuild independently of residency "
          "polling");
    CHECK(publication.NeedsRebuild(requested, false, false),
          "failure before publication leaves the same request eligible for retry");
    CHECK(!publication.NeedsRebuild(initial, true, false),
          "failed requests retain the formerly published revision");
  }
  GroundRevision moreTiles = initial;
  ++moreTiles.ResidentTiles;
  CHECK(!publication.NeedsRebuild(moreTiles, false, false) &&
            publication.NeedsRebuild(moreTiles, true, false),
        "residency-only changes follow the explicit tile-arrival policy");
  CHECK(!publication.NeedsRebuild(initial, false, true) &&
            publication.NeedsRebuild(initial, true, true),
        "missing neighbour rims remain eligible when residency updates are requested");
  GroundRevision playable = initial;
  playable.Quality = GroundQuality::Playable;
  CHECK(publication.Publish(playable) && !publication.NeedsRebuild(playable, false, false) &&
            publication.NeedsRebuild(initial, false, false),
        "a playable publication satisfies playability but remains eligible for refinement");
  CHECK(publication.Publish(initial) && !publication.NeedsRebuild(playable, false, false),
        "a refined publication also satisfies the same playable coverage request");
  GroundRevision wider = initial;
  wider.Coverage.VisualRadiusM = 8000.0;
  CHECK(publication.NeedsRebuild(wider, false, false),
        "a coverage change rebuilds even when its quality label is unchanged");
  CHECK(publication.BeginCapture() && !publication.CanPublish(),
        "capture closes the publication boundary around the complete revision");
  CHECK(!publication.Publish(changed[2]) && publication.Current()->Footprints == initial.Footprints,
        "a late ground revision remains unpublished while capture holds the prior revision");
  publication.EndCapture();
  CHECK(publication.CanPublish() && publication.Publish(changed[2]),
        "releasing capture resumes ground publication");
  CHECK(publication.Current()->Footprints == 4 &&
            !publication.NeedsRebuild(changed[2], false, false),
        "a successful retry advances the published building revision");
  publication.Reset();
  CHECK(!publication.Current() && publication.NeedsRebuild(changed[2], false, false),
        "redeclaration invalidates publication even when its input revision repeats");
  return Report();
}
