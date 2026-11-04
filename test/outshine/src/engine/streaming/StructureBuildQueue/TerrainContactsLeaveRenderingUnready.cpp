#include "BuildingQueueFixture.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Test;

bool Collect(BuildingQueueFixture &fixture, StructureBuildQueue::HeightRequirement heights) {
  auto &prints = fixture.Stack.Footprints();
  size_t posted = 0;
  for (int attempt = 0; attempt < 100; ++attempt) {
    if (posted == 0) {
      posted = fixture.Queue.Posts(fixture.Stack,
                                   prints,
                                   fixture.Eye,
                                   fixture.Source,
                                   1,
                                   heights,
                                   std::nullopt,
                                   StructureBuildQueue::BuildPurpose::TerrainContacts);
    }
    auto ready = fixture.Queue.NextLandings(fixture.Stack,
                                            prints,
                                            fixture.Eye,
                                            fixture.Source,
                                            1,
                                            heights,
                                            std::nullopt,
                                            StructureBuildQueue::BuildPurpose::TerrainContacts);
    CHECK(ready, "contact-only queue work has a terminal result");
    if (!ready) { return false; }
    if (!ready->empty()) {
      const auto &tile = *ready->front().Baked;
      CHECK(tile.Built.WallRun.empty() && tile.Built.RoofRun.empty() && !tile.SelectedView &&
                tile.Prints.size() == 1 && ready->front().SourceComplete,
            "physical metadata completes before any render page exists");
      fixture.Queue.CommitsLandings(prints, *ready);
      return true;
    }
    if (posted == 0 && prints.SourceRefinementComplete() &&
        fixture.Queue.SourcesComplete(fixture.Stack, prints)) {
      return true;
    }
    (void)fixture.Queue.AwaitSlice(0.02);
  }
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  BuildingQueueFixture fixture;
  if (!fixture.Open()) { return Report(); }
  CHECK(Collect(fixture, StructureBuildQueue::HeightRequirement::AllowFallback),
        "initial source contacts can be assembled independently of rendering");
  auto &prints = fixture.Stack.Footprints();
  prints.BeginRefinement();
  CHECK(Collect(fixture, StructureBuildQueue::HeightRequirement::FineOnly),
        "contact refinement establishes the exact physical source revision");
  CHECK(fixture.Queue.SourcesComplete(fixture.Stack, prints),
        "qualified contact inputs complete source readiness");
  CHECK(!fixture.Queue.Complete(fixture.Stack, prints, fixture.Eye, [](uint32_t) { return false; }),
        "qualified contacts never substitute for missing rendering coverage");
  return Report();
}
