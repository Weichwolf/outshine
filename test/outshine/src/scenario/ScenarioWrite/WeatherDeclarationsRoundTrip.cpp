#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <array>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array fields{&Scenario::Weather::CloudCover,
                              &Scenario::Weather::CloudLow,
                              &Scenario::Weather::CloudMid,
                              &Scenario::Weather::CloudHigh,
                              &Scenario::Weather::CloudBaseAglM,
                              &Scenario::Weather::WindDeg,
                              &Scenario::Weather::WindMs,
                              &Scenario::Weather::Haze};
  for (const auto &weather : std::array{Scenario::Weather{.CloudCover = 0.123456789012345,
                                                          .CloudLow = 0.25,
                                                          .CloudMid = 0.5,
                                                          .CloudHigh = 0.75,
                                                          .CloudBaseAglM = 1234.56789012345,
                                                          .WindDeg = 271.234567890123,
                                                          .WindMs = 12.3456789012345,
                                                          .Haze = 2.5},
                                        Scenario::Weather{.Haze = 0.0}}) {
    Scenario::Document source;
    source.Ground.Declared = true;
    source.Ground.Sky = weather;
    const auto text = WriteScenario(source);
    CHECK(text.has_value(), "weather export succeeds");
    if (!text) { continue; }
    Scenario::Document copy;
    std::string error;
    CHECK(ReadScenario(text->data(), text->size(), copy, error), error.c_str());
    CHECK(copy.Ground.Declared, "world participation preserved");
    for (const auto field : fields) {
      CHECK(copy.Ground.Sky.*field == weather.*field, "each weather field survives exactly");
    }
  }
  return Report();
}
