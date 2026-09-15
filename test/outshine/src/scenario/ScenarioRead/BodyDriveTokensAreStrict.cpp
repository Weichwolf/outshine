#include "ScenarioRead.h"
#include "Check.h"
#include <string>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document kept;
  kept.Named.Name = "kept";
  std::string error;
  for (const std::string_view field :
       {"peakNm", "peakN", "ratio", "circleM", "axisX", "axisY", "axisZ"}) {
    for (const std::string_view token : {"", "garbage", "1suffix", "nan", "inf", "1e9999"}) {
      const std::string xml = "<scenario><body><actuator does=\"torque\" " + std::string(field) +
                              "=\"" + std::string(token) + "\"/></body></scenario>";
      CHECK(!ReadScenario(xml.data(), xml.size(), kept, error),
            "malformed or nonfinite actuator number rejects import");
      CHECK(kept.Named.Name == "kept", "failed import preserves previous declaration");
    }
  }
  for (const std::string_view attributes :
       {"peakNm=\"-1\"", "turns=\"no\" peakNm=\"1\"", "turns=\"yes\" peakN=\"1\"", "axisY=\"0\""}) {
    const std::string xml = "<scenario><body><actuator does=\"torque\" " + std::string(attributes) +
                            "/></body></scenario>";
    CHECK(!ReadScenario(xml.data(), xml.size(), kept, error),
          "native channel and magnitude contract applies to XML");
  }
  constexpr std::string_view valid =
      R"(<scenario><body><actuator does="steer" turns="no" peakN="0" ratio="-2" axisZ="-1"/></body></scenario>)";
  CHECK(ReadScenario(valid.data(), valid.size(), kept, error), error.c_str());
  CHECK(kept.Bodies.size() == 1 && kept.Bodies[0].Driven.size() == 1 &&
            !kept.Bodies[0].Driven[0].Turns && kept.Bodies[0].Driven[0].Ratio == -2,
        "zero linear drive survives strict import");
  return Report();
}
