#include "ScenarioLoading.h"
#include "Engine.h"
#include "Check.h"
#include "ScenarioJson.h"
#include <ios>
#include <filesystem>
#include <fstream>
#include <string>
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
    output << "<scenario name=\"oversized\"/>" << std::string(kMostScenarioBytes, ' ');
  }
  const auto oversized = outshine::Test::LoadAndDeclareScenario(engine, path);
  CHECK(!oversized && oversized.error().ends_with("file exceeds byte budget") &&
            engine.declaration().Named.Name != "oversized",
        "scenario loading enforces its budget before publishing a declaration");
  const std::string layerPath = path + ".layer";
  {
    std::ofstream layer(layerPath, std::ios::binary);
    const std::string body = "<scenario/>";
    layer << body << std::string((kMostScenarioBytes / 2) - body.size(), ' ');
    std::ofstream root(path, std::ios::binary);
    root << R"(<scenario name="layered"><layer path=")" << layerPath << "\"/><layer path=\""
         << layerPath << "\"/></scenario>";
  }
  const auto cumulative = outshine::Test::LoadAndDeclareScenario(engine, path);
  CHECK(!cumulative && cumulative.error().ends_with("file exceeds byte budget") &&
            engine.declaration().Named.Name != "layered",
        "selected layers share the root input budget instead of receiving individual budgets");
  std::filesystem::remove(layerPath);
  std::filesystem::remove_all(directory);
  return Report();
}
