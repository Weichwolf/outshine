#include <Outshine.h>
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-layers-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "scratch directory created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const auto write = [&](const char *name, std::string_view text) {
    std::ofstream file(directory + "/" + name);
    file << text;
    file.close();
    CHECK(!file.fail(), "fixture written completely");
  };
  write("layer.scn",
        R"(<scenario><placements><place asset="added" x="2"/></placements></scenario>)");
  write(
      "root.scn",
      R"(<scenario active="day"><layer id="overlay" path="layer.scn" set="day"/><layer path="missing.scn" set="night"/><placements><place asset="base" x="1"/></placements></scenario>)");
  Engine engine;
  CHECK(engine.readScenario(directory + "/root.scn").has_value(),
        "selected layer resolves; inactive missing file is not read");
  CHECK(engine.declaration().Layers.empty(),
        "resolved declaration contains no executable layer references");
  CHECK(engine.declaration().Placements.size() == 2, "layer placement appended exactly once");
  const auto snapshot = engine.writeScenario();
  CHECK(snapshot.has_value(), "resolved snapshot exports");
  if (snapshot) {
    CHECK(snapshot->find("<layer ") == std::string::npos, "snapshot has no source dependencies");
    write("snapshot.scn", *snapshot);
    Engine withSources;
    CHECK(withSources.readScenario(directory + "/snapshot.scn").has_value(),
          "snapshot reloads while the original layer still exists");
    CHECK(withSources.declaration().Placements.size() == 2,
          "existing layer is not applied again on snapshot reload");
    CHECK(std::filesystem::remove(directory + "/layer.scn"),
          "source layer removed for independent reload");
    Engine replay;
    CHECK(replay.readScenario(directory + "/snapshot.scn").has_value(),
          "snapshot reloads without source layer");
    CHECK(replay.declaration().Placements.size() == 2,
          "reload does not append duplicate placements");
    if (replay.declaration().Placements.size() == 2) {
      CHECK(replay.declaration().Placements[0].Asset == "base" &&
                replay.declaration().Placements[1].Asset == "added",
            "resolved order retained");
    }
    const auto before = replay.writeScenario();
    CHECK(!replay.readScenario(directory + "/root.scn"), "missing active layer is still an error");
    CHECK(replay.writeScenario() == before, "failed resolution preserves active declaration");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "scratch files removed");
  return Report();
}
