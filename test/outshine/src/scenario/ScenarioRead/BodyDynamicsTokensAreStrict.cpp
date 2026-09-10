#include "ScenarioRead.h"
#include "Check.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const auto *value : {"nan", "inf", "1e3000", "1x", ""}) {
    for (const auto *field :
         {"massKg", "ixx", "iyy", "izz", "x", "y", "z", "qx", "qy", "qz", "qw"}) {
      const std::string name(field);
      const std::string attribute = name + "=\"" + value + "\"";
      const auto body = name == "massKg"
                            ? "<body " + attribute + "/>"
                            : "<body><" + std::string(name.starts_with("i") ? "inertia" : "at") +
                                  " " + attribute + "/></body>";
      const auto text = "<scenario>" + body + "</scenario>";
      Scenario::Document target;
      target.Named.Name = "kept";
      std::string error;
      CHECK(!ReadScenario(text.data(), text.size(), target, error),
            "malformed dynamic value rejected");
      CHECK(target.Named.Name == "kept", "failed import preserves document");
    }
  }
  const std::string valid = "<scenario><body massKg=\"2\"><at x=\"-3\" qw=\"-1\"/><inertia "
                            "ixx=\"1\" iyy=\"2\" izz=\"3\"/></body></scenario>";
  Scenario::Document target;
  std::string error;
  CHECK(ReadScenario(valid.data(), valid.size(), target, error), error.c_str());
  CHECK(target.Bodies.size() == 1 && target.Bodies[0].MassKg == 2 &&
            target.Bodies[0].Stands.AtM[0] == -3 && target.Bodies[0].InertiaKgM2[2] == 3,
        "valid dynamic values retained");
  const std::string driven =
      "<scenario><body><actuator does=\"torque\" peakNm=\"7\" opposes=\"1\"/></body></scenario>";
  CHECK(ReadScenario(driven.data(), driven.size(), target, error), error.c_str());
  CHECK(target.Bodies.size() == 1 && target.Bodies[0].Driven.size() == 1 &&
            target.Bodies[0].Driven[0].Opposes && target.Bodies[0].Driven[0].Turns &&
            target.Bodies[0].Driven[0].PeakNm == 7 && target.Bodies[0].Driven[0].AxisXyz[1] == 1,
        "torque drive retains its magnitude, opposition and default axis");
  for (const auto *attributes : {"does=\"unknown\"", "does=\"torque\" peakN=\"1\" peakNm=\"1\""}) {
    const auto text =
        std::string("<scenario><body><actuator ") + attributes + "/></body></scenario>";
    target.Named.Name = "kept";
    CHECK(!ReadScenario(text.data(), text.size(), target, error), "invalid drive rejected");
    CHECK(target.Named.Name == "kept", "drive failure preserves document");
  }
  return Report();
}
