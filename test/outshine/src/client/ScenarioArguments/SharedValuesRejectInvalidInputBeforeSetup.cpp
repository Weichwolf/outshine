#include "src/client/ScenarioArguments.h"
#include "Check.h"
#include <array>
#include <string_view>

int main() {
  using namespace outshine::Client;
  using namespace outshine::Test;
  constexpr double kFractionalBudgetS = 180.5;
  const auto budget = ReadPreloadSeconds("180.5");
  CHECK(budget && *budget == kFractionalBudgetS, "fractional preparation budget is preserved");
  for (const auto *invalid :
       std::array{"", "0", "-1", "nan", "inf", "1e999", "15s", " 15", "15 "}) {
    CHECK(!ReadPreloadSeconds(invalid), "invalid budget is rejected before setup");
  }
  CHECK(!ValidCacheDirectory("") && !ValidCacheDirectory("--offline") &&
            ValidCacheDirectory("/tmp/outshine-isolated-cache"),
        "cache paths are nonempty and cannot consume the next flag");
  constexpr auto input = R"({"world":{"vegetation":false}})";
  std::string_view overrides;
  CHECK(ReadScenarioOverrides(input, overrides) && overrides.data() == input,
        "one JSON object stays borrowed");
  CHECK(!ReadScenarioOverrides("{}", overrides) && overrides == input,
        "a duplicate override does not replace the original object");
  for (const auto *invalid : std::array{"", "[]", "--offline"}) {
    std::string_view destination;
    CHECK(!ReadScenarioOverrides(invalid, destination) && destination.empty(),
          "invalid override shape leaves its destination unchanged");
  }
  return Report();
}
