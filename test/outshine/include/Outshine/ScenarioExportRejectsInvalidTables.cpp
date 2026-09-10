#include <Outshine.h>
#include "Check.h"
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document valid;
  valid.Tables = {
      {.Id = "state", .Columns = {"key", "value"}, .Types = {false, true}, .Rows = {{"one", "2"}}}};
  Engine engine;
  CHECK(engine.declare(valid) && engine.assemble(), "valid simulation assembled");
  const auto *simulation = &engine.entities();
  std::vector<Scenario::Document> invalid(3, valid);
  invalid[0].Tables[0].Types.push_back(true);
  invalid[1].Tables[0].Rows[0][1] = "infinity";
  invalid[2].Tables[0].Rows.push_back(invalid[2].Tables[0].Rows[0]);
  for (const auto &document : invalid) {
    CHECK(engine.declare(document).has_value(), "table validation occurs after declaration");
    const auto exported = engine.writeScenario();
    CHECK(!exported, "invalid table is rejected instead of normalized during export");
    if (!exported) { CHECK(!exported.error().empty(), "export returns an owned diagnostic"); }
    CHECK(&engine.entities() == simulation, "export rejection preserves simulation identity");
    const auto repeated = engine.writeScenario();
    CHECK(repeated == exported, "export failure leaves the declaration unchanged");
    CHECK(!engine.assemble(), "assembly agrees with export rejection");
  }
  CHECK(engine.declare(valid).has_value(), "valid declaration replaces invalid candidate");
  const auto exported = engine.writeScenario();
  CHECK(exported && exported->find("<tables>") != std::string::npos,
        "valid retry returns serialized table data");
  CHECK(&engine.entities() == simulation, "successful export does not reassemble simulation");
  return Report();
}
