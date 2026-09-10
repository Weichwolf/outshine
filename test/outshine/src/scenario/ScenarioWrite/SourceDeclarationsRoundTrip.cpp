#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <limits>
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document source;
  source.Providers = {{.Kind = "terrain",
                       .Pin = "snapshot & <a>\"\t",
                       .Rank = std::numeric_limits<int>::min(),
                       .WhenAbsent = "hand over"},
                      {.Kind = "vector",
                       .Pin = "other'\n",
                       .Rank = std::numeric_limits<int>::max(),
                       .WhenAbsent = "fail"},
                      {.Kind = "stars"}};
  source.Compositors = {{.Kind = "detail & <a>", .BudgetPx = 123.45678901234567, .On = false},
                        {.Kind = "default", .BudgetPx = 0, .On = true}};
  const auto text = WriteScenario(source);
  CHECK(text.has_value(), "export succeeds");
  if (!text) { return Report(); }
  Scenario::Document copy;
  std::string error;
  CHECK(ReadScenario(text->data(), text->size(), copy, error), error.c_str());
  CHECK(copy.Providers.size() == source.Providers.size(), "provider count preserved");
  CHECK(copy.Compositors.size() == source.Compositors.size(), "compositor count preserved");
  if (copy.Providers.size() == source.Providers.size()) {
    for (size_t i = 0; i < source.Providers.size(); ++i) {
      const auto &a = source.Providers[i];
      const auto &b = copy.Providers[i];
      CHECK(a.Kind == b.Kind && a.Pin == b.Pin && a.Rank == b.Rank && a.WhenAbsent == b.WhenAbsent,
            "provider order and all fields preserved");
    }
  }
  if (copy.Compositors.size() == source.Compositors.size()) {
    for (size_t i = 0; i < source.Compositors.size(); ++i) {
      const auto &a = source.Compositors[i];
      const auto &b = copy.Compositors[i];
      CHECK(a.Kind == b.Kind && a.BudgetPx == b.BudgetPx && a.On == b.On,
            "compositor order and all fields preserved");
    }
  }
  for (const double invalid :
       {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    auto candidate = source;
    candidate.Compositors.front().BudgetPx = invalid;
    CHECK(!WriteScenario(candidate), "export rejects invalid compositor budget");
  }
  for (const std::string_view budget : {"-1", "nan", "inf", "1e999", "1junk", "", " 1"}) {
    const std::string invalid = "<scenario><compositors><compositor kind='bad' budgetPx='" +
                                std::string(budget) + "'/></compositors></scenario>";
    CHECK(!ReadScenario(invalid.data(), invalid.size(), copy, error),
          "import rejects invalid budget token");
    CHECK(copy.Compositors.size() == source.Compositors.size() &&
              copy.Compositors.front().BudgetPx == source.Compositors.front().BudgetPx,
          "invalid import preserves previous compositor list");
  }
  CHECK(ReadScenario(text->data(), text->size(), copy, error) && error.empty(),
        "valid import retry succeeds");
  return Report();
}
