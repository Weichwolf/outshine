#include "ScenarioWrite.h"
#include "Check.h"
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document valid;
  valid.Tables = {
      {.Id = "state", .Columns = {"key", "value"}, .Types = {false, true}, .Rows = {{"one", "2"}}}};
  CHECK(WriteScenario(valid), "valid typed tables can be serialized directly");
  std::vector<Scenario::Document> invalid(3, valid);
  invalid[0].Tables[0].Types.push_back(true);
  invalid[1].Tables[0].Rows[0][1] = "infinity";
  invalid[2].Tables[0].Rows.push_back(invalid[2].Tables[0].Rows[0]);
  for (const auto &document : invalid) {
    const auto written = WriteScenario(document);
    CHECK(!written && !written.error().empty(),
          "excess types, nonfinite numbers and duplicate keys reject direct serialization");
    CHECK(WriteScenario(document) == written, "rejection leaves the invalid input unchanged");
  }
  return Report();
}
