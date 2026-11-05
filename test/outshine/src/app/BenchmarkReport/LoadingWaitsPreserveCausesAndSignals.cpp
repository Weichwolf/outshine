#include "BenchmarkReport.h"
#include "Check.h"
#include <nlohmann/json.hpp>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  App::ScenarioBenchmarkReport report;
  report.Prepared.PreloadPumps = 23;
  report.Prepared.PreloadFlushes = 7;
  report.Prepared.PreloadAwaits = 19;
  report.Prepared.Waited = {.StructureMs = 12.25,
                            .ClassMs = 2,
                            .TileMs = 50,
                            .WorldWorkerMs = 100,
                            .IdleMs = 3,
                            .StructureCalls = 3,
                            .ClassCalls = 5,
                            .TileCalls = 1,
                            .WorldWorkerCalls = 2,
                            .IdleCalls = 3,
                            .StructureSignals = 2,
                            .ClassSignals = 4,
                            .TileSignals = 0,
                            .WorldWorkerSignals = 0,
                            .TileNoOutstandingMs = 50,
                            .TileNoOutstandingCalls = 1};
  report.Finished.Waited.WorldWorkerSignals = 9;
  const auto json = nlohmann::json::parse(App::ScenarioBenchmarkJson(report), nullptr, false);
  CHECK(!json.is_discarded(), "loading observations serialize without a measured frame");
  if (json.is_discarded()) { return Report(); }
  const auto &prepared = json["loading"]["prepared"];
  CHECK(prepared["preload_pumps"] == 23 && prepared["preload_flushes"] == 7 &&
            prepared["preload_awaits"] == 19,
        "progress-loop counts accompany accumulated durations");
  const auto &waits = prepared["waits"];
  CHECK(waits["structure"]["ms"] == 12.25 && waits["structure"]["calls"] == 3 &&
            waits["structure"]["signals"] == 2 && waits["classification"]["signals"] == 4,
        "source-specific wakeups retain their distinct causes and counts");
  CHECK(waits["tiles"]["ms"] == 50 && waits["tiles"]["signals"] == 0 &&
            waits["world_worker"]["ms"] == 100 && waits["world_worker"]["signals"] == 0 &&
            waits["idle"]["calls"] == 3 && waits["tiles_no_outstanding"]["calls"] == 1,
        "timeouts, idle backoff and waits without outstanding requests stay distinguishable");
  CHECK(json["loading"]["finished"]["waits"]["world_worker"]["signals"] == 9 &&
            waits["world_worker"]["signals"] == 0,
        "prepared and final observations cannot substitute for each other");
  return Report();
}
