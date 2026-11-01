#include <Engine.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scene;
  scene.Tables.push_back(
      {.Id = "stats", .Columns = {"id", "value"}, .Types = {false, true}, .Rows = {{"a", "1"}}});
  Engine engine;
  CHECK(engine.loadScenario(scene), "valid table assembly publishes");
  const auto *previous = &engine.entities();
  scene.Tables[0].Rows[0][1] = "inf";
  CHECK(!engine.loadScenario(scene), "startup rejects a nonfinite numeric cell");
  CHECK(&engine.entities() == previous, "rejected table preserves the published simulation");
  scene.Tables[0].Rows[0][1] = "2";
  CHECK(engine.loadScenario(scene), "corrected table can be assembled");
  return Report();
}
