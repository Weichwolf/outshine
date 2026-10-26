#include "ScenarioLoading.h"
#include "EngineFactory.h"
#include <Engine.h>
#include "Check.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-preload-phases-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated cache directory created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }

  {
    auto engine = App::CreateEngine();
    const bool assembled =
        engine.setRoots(
            {.Shipped = "data", .Cache = directory, .Offline = true, .AssetCache = {}}) &&
        outshine::Test::LoadAndDeclareScenario(
            engine, "test/outshine/data/diagnostic-places/Hockenheimring.scenario") &&
        engine.assemble();
    CHECK(assembled, "offline world assembles from pinned local OSM");
    if (!assembled) { return Report(); }
    const auto ready = engine.preload(0.1);
    const Loading loading = engine.loading();
    CHECK(!ready && !engine.settled(), "empty offline cache cannot make the world playable");
    CHECK(loading.PreloadPumps > 0 && loading.PreloadMs >= 100.0,
          "timed preload pumps until its declared deadline");
    CHECK(loading.PreloadPumpMs > 0.0 &&
              loading.PreloadPumpMs + loading.PreloadFlushMs + loading.PreloadAwaitMs <=
                  loading.PreloadMs,
          "disjoint measured phases fit within total preload wall time");
    const auto &waited = loading.Waited;
    CHECK(waited.StructureMs + waited.ClassMs + waited.TileMs + waited.WorldWorkerMs +
                      waited.IdleMs <=
                  loading.PreloadAwaitMs &&
              waited.StructureSignals <= waited.StructureCalls &&
              waited.ClassSignals <= waited.ClassCalls && waited.TileSignals <= waited.TileCalls &&
              waited.WorldWorkerSignals <= waited.WorldWorkerCalls,
          "wait-source accounting is disjoint and signals never exceed calls");
    CHECK(waited.TileNoOutstandingMs == 0.0 && waited.TileNoOutstandingCalls == 0,
          "preload never waits on an empty tile pool");
    const auto refined = engine.preload(0.0, WorldQuality::Refined);
    CHECK(!refined && !engine.settled(WorldQuality::Refined) && refined.error().contains("pending"),
          "refined preload refuses an incomplete world and describes pending work");
  }

  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated cache directory removed");
  return Report();
}
