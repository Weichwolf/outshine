#ifndef OUTSHINE_TEST_SCENARIOSTARTUP_H
#define OUTSHINE_TEST_SCENARIOSTARTUP_H

#include "Engine.h"

namespace outshine::Test {

[[nodiscard]] inline Result LoadFreshScenario(const Scenario::Document &scenario) {
  Engine engine;
  return engine.loadScenario(scenario);
}

}
#endif
