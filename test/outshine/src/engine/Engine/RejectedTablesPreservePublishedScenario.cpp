#include <Engine.h>
#include "Check.h"
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document valid;
  valid.Tables = {
      {.Id = "state", .Columns = {"key", "value"}, .Types = {false, true}, .Rows = {{"one", "2"}}}};
  Engine engine;
  CHECK(engine.loadScenario(valid), "valid simulation assembled");
  const auto *simulation = &engine.entities();
  const auto published = engine.writeScenario();
  CHECK(published, "the active declaration can be exported");
  std::vector<Scenario::Document> invalid(3, valid);
  invalid[0].Tables[0].Types.push_back(true);
  invalid[1].Tables[0].Rows[0][1] = "infinity";
  invalid[2].Tables[0].Rows.push_back(invalid[2].Tables[0].Rows[0]);
  for (const auto &document : invalid) {
    CHECK(!engine.loadScenario(document), "startup rejects invalid tables");
    const auto exported = engine.writeScenario();
    CHECK(exported && exported == published,
          "an invalid pending table cannot replace the published scenario export");
    CHECK(&engine.entities() == simulation, "export preserves active simulation identity");
    const auto repeated = engine.writeScenario();
    CHECK(repeated == exported, "repeated export leaves the active declaration unchanged");
    CHECK(engine.writeScenario() == published && &engine.entities() == simulation,
          "failed assembly preserves the same exported declaration and simulation");
    CHECK(engine.advance(), "the previously published simulation remains usable");
  }
  CHECK(engine.loadScenario(valid).has_value(), "valid declaration replaces invalid candidate");
  const auto exported = engine.writeScenario();
  CHECK(exported && exported->contains("<tables>"), "valid retry returns serialized table data");
  return Report();
}
