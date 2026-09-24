#include <Outshine.h>
#include "Check.h"

#include <cstdlib>
#include <filesystem>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-preload-phases-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated cache directory created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }

  {
    Engine engine;
    const bool assembled = engine.setRoots({.Assets = "src/assets/drive",
                                            .Shipped = "src/assets",
                                            .Cache = directory,
                                            .Offline = true}) &&
                           engine.readScenario("src/assets/places/Hockenheimring.scenario") &&
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
  }

  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated cache directory removed");
  return Report();
}
