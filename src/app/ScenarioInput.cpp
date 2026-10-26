#include "ScenarioInput.h"
#include "ScenarioLoader.h"
#include "WorldSourcePolicy.h"
#include <utility>
#include <expected>
#include <string>
#include <string_view>

namespace outshine::App {

std::expected<Scenario::Document, std::string> ReadScenarioInput(
    std::string_view source, std::string_view overrides, std::string_view resourceDirectory) {
  auto loaded = App::LoadScenario(
      {.Source = source, .Overrides = overrides, .ResourceDirectory = resourceDirectory});
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (const auto sources = ConfigureWorldSources(loaded->Document); !sources) {
    return std::unexpected(sources.error());
  }
  return std::move(loaded->Document);
}

}
