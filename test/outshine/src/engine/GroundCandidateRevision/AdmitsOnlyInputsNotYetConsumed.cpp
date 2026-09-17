#include "GroundCandidateRevision.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const GroundRevision initial{
      .Region = 17, .ResidentTiles = 4, .Classes = 2, .Footprints = 3, .Projection = {0, 1.0, 2.0}};
  GroundCandidateRevision candidate(initial);
  GroundRevision latest = initial;
  latest.Classes = 5;
  latest.Footprints = 7;
  CHECK(candidate.Accepts(latest) && candidate.Current().Classes == 5 &&
            candidate.Current().Footprints == 7,
        "a candidate admits data that no completed phase consumed");
  candidate.AdvancesTo(GroundCandidateRevision::Stage::NeedsGroundSurface);
  ++latest.Footprints;
  CHECK(candidate.Accepts(latest) && candidate.Current().Footprints == 8,
        "footprints remain admissible before modelling");
  ++latest.Classes;
  CHECK(!candidate.Accepts(latest), "classes rebuild after classification consumed their snapshot");
  --latest.Classes;
  candidate.AdvancesTo(GroundCandidateRevision::Stage::NeedsModels);
  ++latest.Footprints;
  CHECK(candidate.Accepts(latest), "footprints remain admissible until modelling consumes them");
  candidate.AdvancesTo(GroundCandidateRevision::Stage::NeedsGeometry);
  ++latest.Footprints;
  CHECK(!candidate.Accepts(latest), "footprints rebuild after modelling consumes them");
  --latest.Footprints;
  ++latest.Region;
  CHECK(!candidate.Accepts(latest), "a region change abandons every partial product");
  --latest.Region;
  latest.Projection[1] = 0.5;
  CHECK(!candidate.Accepts(latest), "a projection change abandons the prepared renderer state");
  return Report();
}
