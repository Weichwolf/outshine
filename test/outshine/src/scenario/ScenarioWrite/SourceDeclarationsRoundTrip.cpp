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
  const auto text = WriteScenario(source);
  CHECK(text.has_value(), "export succeeds");
  if (!text) { return Report(); }
  Scenario::Document copy;
  std::string error;
  CHECK(ReadScenario(text->data(), text->size(), copy, error), error.c_str());
  CHECK(copy.Providers.size() == source.Providers.size(), "provider count preserved");
  if (copy.Providers.size() == source.Providers.size()) {
    for (size_t i = 0; i < source.Providers.size(); ++i) {
      const auto &a = source.Providers[i];
      const auto &b = copy.Providers[i];
      CHECK(a.Kind == b.Kind && a.Pin == b.Pin && a.Rank == b.Rank && a.WhenAbsent == b.WhenAbsent,
            "provider order and all fields preserved");
    }
  }
  CHECK(ReadScenario(text->data(), text->size(), copy, error) && error.empty(),
        "valid import retry succeeds");
  return Report();
}
