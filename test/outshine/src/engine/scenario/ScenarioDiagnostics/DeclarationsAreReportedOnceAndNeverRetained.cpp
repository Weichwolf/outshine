#include "ScenarioDiagnostics.h"
#include "Scenario.h"
#include "Check.h"
#include <algorithm>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(DescribeScenarioDeclarations({}).empty(), "empty declarations have no diagnostics");
  Scenario::Document declared;
  declared.Tables.resize(2);
  declared.Buses.resize(3);
  declared.Sounds.resize(4);
  declared.Assets.resize(2);
  declared.Assets[0].Uri = "still";
  declared.Assets[0].Animation = Scenario::AssetAnimation::Ignore;
  declared.Assets[1].Uri = "driven";
  declared.Assets[1].Animation = Scenario::AssetAnimation::Driven;
  const auto described = DescribeScenarioDeclarations(declared);
  for (const char *expected : {"2 tables", "3 audio buses", "4 sounds"}) {
    CHECK(std::ranges::count(described, expected) == 1,
          "each declared category contributes exactly one summary");
  }
  CHECK(described.size() == 6, "declaration counts and both animation policies appear once");
  CHECK(std::ranges::count_if(described,
                              [](const std::string &line) {
                                return line.contains("asset 'still'") && line.contains("IGNORED");
                              }) == 1,
        "explicit still-animation policy remains visible");
  CHECK(std::ranges::count_if(described,
                              [](const std::string &line) {
                                return line.contains("asset 'driven'") && line.contains("DRIVEN");
                              }) == 1,
        "simulation-driven animation policy remains visible");
  CHECK(DescribeScenarioDeclarations({}).empty() &&
            DescribeScenarioDeclarations(declared) == described,
        "independent descriptions retain no state and leave the input unchanged");
  return Report();
}
