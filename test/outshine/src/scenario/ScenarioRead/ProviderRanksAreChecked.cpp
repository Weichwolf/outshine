#include "ScenarioRead.h"
#include "Check.h"
#include <limits>
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document document;
  document.Named.Name = "previous";
  document.Providers.push_back({.Kind = "previous", .Priority = 7});
  std::string error;
  for (const std::string_view rank : {"2147483648",
                                      "-2147483649",
                                      "99999999999999999999999",
                                      "1tail",
                                      "1.5",
                                      "1e2",
                                      " 1",
                                      "1 ",
                                      "",
                                      "+",
                                      "+-1",
                                      "--1"}) {
    const std::string text = "<scenario name='new'><providers><provider kind='valid'/>"
                             "<provider kind='bad' rank='" +
                             std::string(rank) + "'/></providers></scenario>";
    CHECK(!ReadScenario(text.data(), text.size(), document, error), "invalid rank rejected");
    CHECK(!error.empty(), "invalid rank diagnosed");
    CHECK(document.Named.Name == "previous" && document.Providers.size() == 1 &&
              document.Providers.front().Kind == "previous" &&
              document.Providers.front().Priority == 7,
          "invalid later rank preserves previous document");
  }
  for (const int rank : {std::numeric_limits<int>::min(), 0, std::numeric_limits<int>::max()}) {
    const std::string text = "<scenario><providers><provider kind='ok' rank='" +
                             std::to_string(rank) + "'/></providers></scenario>";
    CHECK(ReadScenario(text.data(), text.size(), document, error) && error.empty(),
          "valid retry succeeds");
    CHECK(document.Providers.size() == 1 && document.Providers.front().Priority == rank,
          "int rank preserved");
  }
  constexpr std::string_view text = "<scenario><providers><provider kind='a' rank='+12'/>"
                                    "<provider kind='b'/></providers></scenario>";
  CHECK(ReadScenario(text.data(), text.size(), document, error), "plus and omitted rank accepted");
  CHECK(document.Providers.size() == 2 && document.Providers[0].Priority == 12 &&
            document.Providers[1].Priority == 0,
        "plus and default rank preserved");
  constexpr std::string_view invalidPolicy =
      "<scenario><providers><provider kind='terrain' whenAbsent='retry'/></providers></scenario>";
  CHECK(!ReadScenario(invalidPolicy.data(), invalidPolicy.size(), document, error) &&
            !error.empty() && document.Providers.size() == 2,
        "unknown absence policy rejects without replacing the previous document");
  return Report();
}
