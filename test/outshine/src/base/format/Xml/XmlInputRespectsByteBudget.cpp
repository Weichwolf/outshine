#include "Xml.h"
#include "ScenarioRead.h"
#include "Check.h"
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t limit = 16u * 1024u * 1024u;
  const std::string prefix = "<scenario><!--";
  const std::string suffix = "--></scenario>";
  std::string text = prefix + std::string(limit - prefix.size() - suffix.size(), 'x') + suffix;
  Xml xml;
  CHECK(xml.Parse(text.data(), text.size()), "exact input byte budget is accepted");
  CHECK(xml.Root().Name() == "scenario", "budget-sized document has its declared root");
  text.insert(prefix.size(), 1, 'x');
  CHECK(!xml.Parse(text.data(), text.size()), "one byte over budget is rejected");
  CHECK(!xml.Error().empty(), "over-budget input reports a diagnostic");
  Scenario::Document scene;
  scene.Named.Name = "preserved";
  std::string error;
  CHECK(!ReadScenario(text.data(), text.size(), scene, error),
        "in-memory scenario cannot bypass the XML input budget");
  CHECK(scene.Named.Name == "preserved", "rejected oversized scenario preserves prior state");
  constexpr std::string_view valid = "<scenario/>";
  CHECK(xml.Parse(valid.data(), valid.size()) && xml.Error().empty(),
        "valid retry succeeds after oversized input");
  return Report();
}
