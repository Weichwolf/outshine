#include "GroundBuildSchedule.h"
#include "Check.h"

int main() {
  using outshine::Core::GroundBuildSchedule;
  using namespace outshine::Test;
  GroundBuildSchedule schedule;
  CHECK(!schedule.Prepared() && schedule.Status() == "sheet-fields",
        "an unopened candidate cannot begin a production phase");
  CHECK(!schedule.AdvanceSheetPhase() && !schedule.AdvanceStage(),
        "an unopened candidate cannot advance terrain or production");
  CHECK(schedule.MarkPrepared(), "preparation opens the candidate exactly once");
  CHECK(!schedule.MarkPrepared(), "a prepared candidate cannot be prepared again");
  CHECK(schedule.Prepared() && schedule.Status() == "sheet-fields",
        "preparation does not skip height source acquisition");
  CHECK(!schedule.AdvanceStage(),
        "production cannot advance while the terrain candidate remains incomplete");
  CHECK(schedule.AdvanceSheetPhase() && schedule.Status() == "sheet-refinement" &&
            schedule.AdvanceSheetPhase() && schedule.AdvanceSheetPhase() &&
            schedule.AdvanceSheetPhase(),
        "terrain phases advance in their declared order");
  CHECK(!schedule.AdvanceSheetPhase(), "a complete terrain candidate cannot advance again");
  constexpr GroundBuildSchedule::Stage kRequired[] = {
      GroundBuildSchedule::Stage::NeedsClasses,
      GroundBuildSchedule::Stage::NeedsGroundSurface,
      GroundBuildSchedule::Stage::NeedsModels,
      GroundBuildSchedule::Stage::NeedsNetwork,
      GroundBuildSchedule::Stage::NeedsRoadAlignments,
      GroundBuildSchedule::Stage::NeedsBakes,
      GroundBuildSchedule::Stage::NeedsCorridors,
      GroundBuildSchedule::Stage::NeedsEarthworks,
      GroundBuildSchedule::Stage::NeedsTerrainMesh,
      GroundBuildSchedule::Stage::NeedsWater,
      GroundBuildSchedule::Stage::NeedsGeometry,
      GroundBuildSchedule::Stage::NeedsPublication};
  for (size_t at = 0; at < std::size(kRequired); ++at) {
    const auto stage = kRequired[at];
    CHECK(schedule.CurrentStage() == stage,
          "a candidate remains at each complete production phase until its owner advances it");
    if (at + 1u < std::size(kRequired)) {
      CHECK(schedule.AdvanceStage(), "only the current production phase can advance");
    }
  }
  CHECK(schedule.Status() == "publication",
        "publication is reachable only after every candidate product phase completes");
  CHECK(!schedule.AdvanceStage(), "publication is terminal until its owner commits or rejects it");
  return Report();
}
