#include <Outshine.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document scene;
  scene.Tables.push_back(
      {.Id = "stats", .Columns = {"id", "value"}, .Types = {false, true}, .Rows = {{"a", "1"}}});
  Engine engine;
  CHECK(engine.declare(scene) && engine.assemble(), "valid table assembly publishes");
  const auto *previous = &engine.entities();
  scene.Tables[0].Rows[0][1] = "inf";
  CHECK(engine.declare(scene).has_value(), "table parsing belongs to assembly");
  CHECK(!engine.assemble(), "nonfinite numeric cell rejects assembly");
  CHECK(&engine.entities() == previous, "rejected table preserves the published simulation");
  scene.Tables[0].Rows[0][1] = "2";
  CHECK(engine.declare(scene) && engine.assemble(), "corrected table can be assembled");
  return Report();
}
