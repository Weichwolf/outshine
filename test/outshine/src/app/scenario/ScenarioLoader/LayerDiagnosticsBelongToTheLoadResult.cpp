#include "ScenarioLoader.h"
#include "Check.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-loader-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated layer directory created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  {
    std::ofstream file(directory + "/selected.scenario");
    file << R"({"placements":{"place":[{"asset":"added"}]}})";
    file.close();
    CHECK(!file.fail(), "selected layer written completely");
  }
  constexpr std::string_view source = R"({"active":"day","layer":[
    {"id":"selected","path":"selected.scenario","set":"day"},
    {"id":"inactive","path":"missing.scenario","set":"night"}]})";
  const auto loaded =
      App::LoadScenario({.Source = source, .Overrides = {}, .ResourceDirectory = directory});
  CHECK(loaded, "inline input resolves its explicit directory without reading inactive files");
  if (loaded) {
    CHECK(loaded->Document.Layers.empty() && loaded->Document.Placements.size() == 1,
          "selected layer is applied and source references are consumed");
    CHECK(std::ranges::any_of(loaded->LayerTrace,
                              [](const std::string &entry) {
                                return entry.contains("selected") && entry.contains("added");
                              }),
          "load result reports selected layer composition");
    CHECK(std::ranges::any_of(loaded->LayerTrace,
                              [](const std::string &entry) {
                                return entry.contains("inactive") && entry.contains("not selected");
                              }),
          "load result reports inactive layer selection");
  }
  const auto independent =
      App::LoadScenario({.Source = "{}", .Overrides = {}, .ResourceDirectory = {}});
  CHECK(independent && independent->LayerTrace.empty(), "later loads retain no prior diagnostics");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated inputs removed");
  return Report();
}
