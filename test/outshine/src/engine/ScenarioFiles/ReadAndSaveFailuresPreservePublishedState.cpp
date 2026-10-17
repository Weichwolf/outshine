#include "Outshine.h"
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
  const std::string path = directory + "/scenario";
  Engine engine;
  {
    std::ofstream output(path, std::ios::binary);
    output << "<scenario name=\"oversized\"/>" << std::string(16u << 20u, ' ');
  }
  const auto oversized = engine.readScenario(path);
  CHECK(!oversized && oversized.error().ends_with("file exceeds byte budget") &&
            engine.declaration().Named.Name != "oversized",
        "public scenario reader enforces its budget before publishing a declaration");
  const std::string layerPath = path + ".layer";
  {
    std::ofstream layer(layerPath, std::ios::binary);
    const std::string body = "<scenario/>";
    layer << body << std::string((8u << 20u) - body.size(), ' ');
    std::ofstream root(path, std::ios::binary);
    root << "<scenario name=\"layered\"><layer path=\"" << layerPath << "\"/><layer path=\""
         << layerPath << "\"/></scenario>";
  }
  const auto cumulative = engine.readScenario(path);
  CHECK(!cumulative && cumulative.error().ends_with("file exceeds byte budget") &&
            engine.declaration().Named.Name != "layered",
        "selected layers share the root input budget instead of receiving individual budgets");
  std::filesystem::remove(layerPath);
  Scenario::Document scene;
  scene.Room = 4;
  Scenario::Kind kind;
  kind.Name = "actor";
  kind.Attributes = {{"health", "23"}};
  scene.Kinds.push_back(kind);
  scene.Instances.push_back({.Of = "actor", .Id = "kept"});
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
