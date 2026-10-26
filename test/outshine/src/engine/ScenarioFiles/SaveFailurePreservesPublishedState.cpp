#include "Engine.h"
#include "ReadTextFile.h"
#include "Check.h"
#include <csignal>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/resource.h>
#include <unistd.h>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-scenario-files-XXXXXX").string();
  if (mkdtemp(directory.data()) == nullptr) {
    CHECK(false, "temporary directory created");
    return Report();
  }
  Engine engine;
  Scenario::Document scene;
  scene.Room = 4;
  Scenario::Kind kind;
  kind.Name = "actor";
  kind.Attributes = {{.Name = "health", .Value = "23"}};
  scene.Kinds.push_back(kind);
  scene.Instances.push_back(
      {.Of = "actor", .Id = "kept", .In = {}, .Stands = {}, .Attributes = {}, .Holds = {}});
  scene.State.push_back({.What = "kept.health"});
  const std::string savePath = directory + "/engine-save";
  const bool ready = engine.declare(scene).has_value() && engine.assemble().has_value() &&
                     engine.save(savePath).has_value();
  CHECK(ready, "public API creates a complete save");
  const auto previousSave = ReadTextFile(savePath, 4096);
  struct rlimit saved{};
  const bool queried = getrlimit(RLIMIT_FSIZE, &saved) == 0;
  CHECK(queried, "file size limit can be inspected");
  if (queried) {
    const auto previousSignal = std::signal(SIGXFSZ, SIG_IGN);
    auto limited = saved;
    limited.rlim_cur = 1;
    const bool changed = setrlimit(RLIMIT_FSIZE, &limited) == 0;
    CHECK(changed, "isolated test process applies file size limit");
    if (changed) {
      if (ready) {
        CHECK(!engine.save(savePath), "public save propagates the publication failure");
        const auto preserved = ReadTextFile(savePath, 4096);
        CHECK(previousSave && preserved && *preserved == *previousSave,
              "failed public save preserves the previous complete snapshot bytes");
      }
      CHECK(setrlimit(RLIMIT_FSIZE, &saved) == 0, "file size limit restored");
    }
    std::signal(SIGXFSZ, previousSignal);
  }
  std::filesystem::remove_all(directory);
  return Report();
}
