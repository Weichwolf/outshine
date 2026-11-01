#include "ScenarioStartup.h"
#include "Check.h"
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document valid;
  valid.Tables = {
      {.Id = "state", .Columns = {"key", "value"}, .Types = {false, true}, .Rows = {{"one", "2"}}}};
  Engine engine;
  CHECK(engine.loadScenario(valid), "valid tables start");
  const auto published = engine.writeScenario();
  CHECK(published && published->contains("<tables>"), "the started tables can be exported");
  CHECK(engine.writeScenario() == published, "repeated export preserves the declaration");
  std::vector<Scenario::Document> invalid(4, valid);
  invalid[0].Tables[0].Types.push_back(true);
  invalid[1].Tables[0].Rows[0][1] = "infinity";
  invalid[2].Tables[0].Rows.push_back(invalid[2].Tables[0].Rows[0]);
  invalid[3].Tables[0].Rows[0][1] = "inf";
  for (const auto &document : invalid) {
    const auto loaded = LoadFreshScenario(document);
    CHECK(!loaded && !loaded.error().empty(), "startup diagnoses invalid table contents");
  }
  return Report();
}
