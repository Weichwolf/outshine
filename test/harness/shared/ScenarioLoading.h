#ifndef OUTSHINE_TEST_SCENARIOLOADING_H
#define OUTSHINE_TEST_SCENARIOLOADING_H

#include "Engine.h"
#include "ScenarioLoader.h"
#include <string_view>
#include <expected>

namespace outshine::Test {

[[nodiscard]] inline Result LoadAndDeclareScenario(Engine &engine,
                                                   std::string_view source,
                                                   std::string_view overrides = {},
                                                   std::string_view resourceDirectory = {}) {
  const auto loaded = App::LoadScenario(
      {.Source = source, .Overrides = overrides, .ResourceDirectory = resourceDirectory});
  if (!loaded) { return std::unexpected(loaded.error()); }
  return engine.declare(loaded->Document);
}

}
#endif
