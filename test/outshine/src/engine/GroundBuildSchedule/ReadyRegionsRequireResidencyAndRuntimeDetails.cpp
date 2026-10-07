#include "GroundBuildSchedule.h"
#include "Check.h"

int main() {
  using outshine::Core::GroundBuildSchedule;
  using namespace outshine::Test;
  GroundBuildSchedule schedule;
  CHECK(!schedule.LoadReadyRegion(), "an unprepared world cannot accept a native region");
  CHECK(schedule.MarkPrepared() && schedule.LoadReadyRegion(),
        "prepared world accepts a ready region");
  CHECK(schedule.HasReadyRegion() &&
            schedule.CurrentSheetPhase() == GroundBuildSchedule::SheetPhase::NeedsMesh,
        "cached height pages must become resident before publication");
  CHECK(!schedule.AdvanceStage(), "ready bytes alone cannot publish a region");
  CHECK(schedule.AdvanceSheetPhase() && schedule.Status() == "models",
        "residency enables runtime material registration");
  CHECK(schedule.AdvanceStage() && schedule.Status() == "structure-bakes",
        "building runtime products are still integrated");
  CHECK(schedule.AdvanceStage() && schedule.Status() == "geometry",
        "ready roads, deformation and water bypass reconstruction");
  CHECK(schedule.AdvanceStage() && schedule.Status() == "publication",
        "GPU geometry precedes world publication");
  CHECK(!schedule.LoadReadyRegion() && !schedule.AdvanceStage(),
        "published schedule cannot accept another region");
  return Report();
}
