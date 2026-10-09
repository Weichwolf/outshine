#include <Outshine.h>
#include "Check.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-json-layers-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "scratch directory created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const auto write = [&](const char *name, std::string_view text) {
    std::ofstream file(directory + "/" + name);
    file << text;
    file.close();
    CHECK(!file.fail(), "fixture written completely");
  };
  write(
      "day.scenario",
      R"({"world":{"lat":50,"lon":8,"sightM":120000,"vegetation":true},"render":{"widthPx":320,"heightPx":240}})");
  constexpr std::string_view source =
      R"({"name":"initial","active":"night","world":{"lat":47,"lon":9,"sightM":240000},"render":{"widthPx":640,"heightPx":480},"layer":[{"path":"day.scenario","set":"day"},{"path":"missing.scenario","set":"night"}]})";
  constexpr std::string_view overrides =
      R"({"name":null,"active":"day","world":{"vegetation":false},"render":{"widthPx":800}})";
  Engine engine;
  CHECK(engine.setRoots({.Assets = directory, .Shipped = directory}).has_value(),
        "inline source has an explicit layer base directory");
  CHECK(engine.readScenario(source, overrides).has_value(),
        "inline JSON selects a JSON layer using overridden metadata");
  CHECK(engine.declaration().Named.Active == "day" && engine.declaration().Named.Name.empty() &&
            engine.declaration().Layers.empty() && !engine.declaration().Ground.VegetationEnabled &&
            engine.declaration().Ground.Origin.LatitudeDeg == 50 &&
            engine.declaration().Ground.Origin.LongitudeDeg == 8 &&
            engine.declaration().Render.Frame.WidthPx == 800 &&
            engine.declaration().Render.Frame.HeightPx == 240,
        "final overrides win while unmentioned layer values survive and references are consumed");
  const auto snapshot = engine.writeScenario();
  CHECK(snapshot.has_value(), "overridden inline declaration exports");
  write("root.scenario", source);
  Engine file;
  CHECK(file.readScenario(directory + "/root.scenario", overrides).has_value(),
        "a JSON file uses its own directory for selected layers");
  CHECK(file.writeScenario() == snapshot, "file and inline sources have identical declarations");
  CHECK(
      engine
          .readScenario(
              source,
              R"({"name":null,"active":"day","world":{"vegetation":false},"render":{"widthPx":800},"layer":[{"path":"day.scenario","set":"day"}]})")
          .has_value(),
      "overrides can replace the selected layer collection");
  CHECK(engine.writeScenario() == snapshot && engine.declaration().Layers.empty(),
        "overridden layer references are consumed rather than reintroduced by final overrides");
  for (std::string_view invalid :
       {R"({"world":{"vegetation":{}}})", R"({"world":{"nonexistent":1}})", R"([])", "{"}) {
    CHECK(!engine.readScenario(source, invalid), "invalid overrides are refused");
    CHECK(engine.writeScenario() == snapshot, "override failure preserves the active declaration");
  }
  CHECK(!engine.readScenario(source), "missing selected layer remains an error without overrides");
  CHECK(engine.writeScenario() == snapshot, "layer failure preserves the active declaration");
  std::string nul = "{}";
  nul += '\0';
  CHECK(!engine.readScenario(nul), "embedded input NUL is rejected");
  CHECK(!engine.readScenario("{}", std::string(16u * 1024u * 1024u + 1u, ' ')),
        "combined scenario input has a bounded byte budget");
  CHECK(!engine.readScenario("{" + std::string(16u * 1024u * 1024u, ' ')),
        "oversized inline input is refused before copying or parsing");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "scratch files removed");
  return Report();
}
