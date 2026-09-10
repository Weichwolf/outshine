#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document source;
  source.State = {{"player.health"}, {"car & <one>.fuel'\""}, {"player.health"}};
  const auto text = WriteScenario(source);
  CHECK(text.has_value(), "export succeeds");
  if (!text) { return Report(); }
  Scenario::Document copy;
  std::string error;
  CHECK(ReadScenario(text->data(), text->size(), copy, error), error.c_str());
  CHECK(copy.State.size() == source.State.size(), "persistence selection count survives");
  if (copy.State.size() == source.State.size()) {
    for (size_t i = 0; i < source.State.size(); ++i) {
      CHECK(copy.State[i].What == source.State[i].What,
            "order, duplicates and escaped names survive");
    }
  }
  CHECK(source.State.front().What == "player.health", "export preserves source");
  return Report();
}
