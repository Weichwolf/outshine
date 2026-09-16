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
  publication.Publish(initial);
  CHECK(!publication.NeedsRebuild(initial, true, false),
        "the successfully published revision needs no duplicate build");
  std::array<GroundRevision, 6> changed{initial, initial, initial, initial, initial, initial};
  ++changed[0].Region;
  ++changed[1].Classes;
  ++changed[2].Footprints;
  changed[3].Projection[0] = 1;
  changed[4].Projection[1] = 0.5;
  changed[5].Projection[2] = 4;
  for (const GroundRevision &requested : changed) {
    CHECK(publication.NeedsRebuild(requested, false, false),
          "region, data and projection changes rebuild independently of residency polling");
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
  publication.Publish(changed[2]);
  CHECK(publication.Current()->Footprints == 4 &&
            !publication.NeedsRebuild(changed[2], false, false),
        "a successful retry advances the published building revision");
  publication.Reset();
  CHECK(!publication.Current() && publication.NeedsRebuild(changed[2], false, false),
        "redeclaration invalidates publication even when its input revision repeats");
  return Report();
}
