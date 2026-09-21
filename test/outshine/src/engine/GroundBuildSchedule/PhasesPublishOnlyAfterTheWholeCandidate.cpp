#include "GroundBuildSchedule.h"
#include "Check.h"

int main() {
  using outshine::Core::GroundBuildSchedule;
  using namespace outshine::Test;
  GroundBuildSchedule schedule;
  CHECK(!schedule.Prepared() && schedule.Status() == "sheet-refinement",
        "an unopened candidate cannot begin a production phase");
  schedule.MarksPrepared();
  CHECK(schedule.Prepared() && schedule.Status() == "sheet-refinement",
        "preparation does not skip terrain refinement");
  schedule.CompletesSheetPhase();
  schedule.CompletesSheetPhase();
  schedule.CompletesSheetPhase();
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
  for (const auto stage : kRequired) {
    CHECK(schedule.NextStage() == stage,
          "a candidate remains at each complete production phase until its owner advances it");
    schedule.CompletesStage();
  }
  CHECK(schedule.Status() == "publication",
        "publication is reachable only after every candidate product phase completes");
  return Report();
}
