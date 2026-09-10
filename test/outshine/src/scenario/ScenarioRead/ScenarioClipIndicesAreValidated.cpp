#include "ScenarioRead.h"
#include "Check.h"
#include <limits>
#include <string>
#include <string_view>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document document;
  document.Named.Name = "previous";
  std::string error;
  for (const std::string_view token : {"",
                                       "-1",
                                       "3junk",
                                       "0.5",
                                       "3.00000000000000001",
                                       "2147483648",
                                       "1e100",
                                       "nan",
                                       "inf",
                                       "-inf"}) {
    const std::string text = "<scenario><assets><asset kind='gltf' uri='x.glb' clip='" +
                             std::string(token) + "'/></assets></scenario>";
    CHECK(!ReadScenario(text.data(), text.size(), document, error), "invalid clip is rejected");
    CHECK(!error.empty() && document.Named.Name == "previous" && document.Assets.empty(),
          "rejected clip reports an error and preserves previous document");
  }
  for (const auto &[token, expected] : {std::pair{"0", 0},
                                        {"3.0", 3},
                                        {"3e1", 30},
                                        {"2147483647", std::numeric_limits<int>::max()}}) {
    const std::string text = "<scenario><assets><asset kind='gltf' uri='x.glb' clip='" +
                             std::string(token) + "'/></assets></scenario>";
    CHECK(ReadScenario(text.data(), text.size(), document, error), "valid clip accepted");
    CHECK(document.Assets.size() == 1 && document.Assets.front().Clip == expected,
          "valid clip retains exact index");
  }
  return Report();
}
