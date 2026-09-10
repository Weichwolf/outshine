#include "ScenarioRead.h"
#include "Check.h"
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto maximum = std::to_string(std::numeric_limits<size_t>::max());
  for (const std::string &value : {std::string("-1"),
                                   std::string("0.5"),
                                   std::string("1x"),
                                   std::string("nan"),
                                   std::string("inf"),
                                   std::string("1e3000"),
                                   std::string(""),
                                   maximum + "0"}) {
    const auto text = "<scenario><scene room=\"" + value + "\"/></scenario>";
    Scenario::Document target;
    target.Named.Name = "kept";
    target.Room = 7;
    std::string error;
    CHECK(!ReadScenario(text.data(), text.size(), target, error), "invalid room count rejected");
    CHECK(!error.empty(), "rejection gives a diagnostic");
    CHECK(target.Named.Name == "kept" && target.Room == 7, "failure preserves document");
  }
  for (const std::string &value : {std::string("0"), std::string("65536"), maximum}) {
    const auto text = "<scenario><scene room=\"" + value + "\"/></scenario>";
    Scenario::Document target;
    std::string error;
    CHECK(ReadScenario(text.data(), text.size(), target, error), error.c_str());
    CHECK(std::to_string(target.Room) == value, "integer retained without floating-point rounding");
  }
  Scenario::Document target;
  target.Room = 7;
  std::string error;
  const std::string absent = "<scenario><scene/></scenario>";
  CHECK(ReadScenario(absent.data(), absent.size(), target, error) && target.Room == 0,
        "absent count defaults to zero");
  return Report();
}
