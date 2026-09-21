#include "GroundBuildSchedule.h"
#include "Check.h"

int main() {
  using outshine::Core::GroundBuildSchedule;
  using namespace outshine::Test;
  GroundBuildSchedule schedule;
  CHECK(!schedule.Prepared() && schedule.Status() == "sheet-refinement",
        "an unopened candidate cannot begin a production phase");
  CHECK(!schedule.CompletesSheetPhase() && !schedule.CompletesStage(),
        "an unopened candidate cannot advance terrain or production");
  CHECK(schedule.MarksPrepared(), "preparation opens the candidate exactly once");
  CHECK(!schedule.MarksPrepared(), "a prepared candidate cannot be prepared again");
  CHECK(schedule.Prepared() && schedule.Status() == "sheet-refinement",
        "preparation does not skip terrain refinement");
  CHECK(!schedule.CompletesStage(),
        "production cannot advance while the terrain candidate remains incomplete");
  CHECK(schedule.CompletesSheetPhase() && schedule.CompletesSheetPhase() &&
            schedule.CompletesSheetPhase(),
        "terrain phases advance in their declared order");
  CHECK(!schedule.CompletesSheetPhase(), "a complete terrain candidate cannot advance again");
  constexpr GroundBuildSchedule::Stage kRequired[] = {
      GroundBuildSchedule::Stage::NeedsClasses,
      GroundBuildSchedule::Stage::NeedsGroundSurface,
      GroundBuildSchedule::Stage::NeedsModels,
      GroundBuildSchedule::Stage::NeedsBakes,
      GroundBuildSchedule::Stage::NeedsCorridors,
      GroundBuildSchedule::Stage::NeedsEarthworks,
      GroundBuildSchedule::Stage::NeedsTerrainMesh,
      GroundBuildSchedule::Stage::NeedsWater,
      GroundBuildSchedule::Stage::NeedsGeometry,
      GroundBuildSchedule::Stage::NeedsPublication};
  for (size_t at = 0; at < std::size(kRequired); ++at) {
    const auto stage = kRequired[at];
    CHECK(schedule.NextStage() == stage,
          "a candidate remains at each complete production phase until its owner advances it");
    if (at + 1u < std::size(kRequired)) {
      CHECK(schedule.CompletesStage(), "only the current production phase can advance");
    }
  }
  CHECK(schedule.Status() == "publication",
        "publication is reachable only after every candidate product phase completes");
  CHECK(!schedule.CompletesStage(),
        "publication is terminal until its owner commits or rejects it");
  return Report();
}
