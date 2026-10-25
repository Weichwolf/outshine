#include "ScenarioInput.h"
#include "EngineFactory.h"
#include "WorldSourcePolicy.h"
#include <Engine.h>
#include <Scenario.h>
#include <expected>
#include <string>
#include <string_view>

namespace outshine::Client {

std::expected<Scenario::Document, std::string>
ReadClientScenario(std::string_view source, std::string_view overrides, const Roots &roots) {
  auto reader = App::CreateEngine();
  if (const auto rooted = reader.setRoots(roots); !rooted) {
    return std::unexpected(rooted.error());
  }
  if (const auto read = reader.readScenario(source, overrides); !read) {
    return std::unexpected(read.error());
  }
  auto declaration = reader.declaration();
  if (const auto sources = ConfigureWorldSources(declaration); !sources) {
    return std::unexpected(sources.error());
  }
  return declaration;
}

}
