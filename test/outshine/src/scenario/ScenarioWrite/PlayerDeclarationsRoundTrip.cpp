#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <array>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const bool declared : {false, true}) {
    Scenario::Document source;
    source.Played = {.Declared = declared,
                     .Is = "body&one",
                     .Starts = "door<two",
                     .View = "view\"three",
                     .EyeHeightM = 1.23456789012345,
                     .WalkMs = 0,
                     .RunMs = 4.56789012345678};
    const auto text = WriteScenario(source);
    CHECK(text.has_value(), "player export succeeds");
    if (!text) { continue; }
    Scenario::Document copy;
    std::string error;
    CHECK(ReadScenario(text->data(), text->size(), copy, error), error.c_str());
    CHECK(copy.Played.Declared, "nondefault player values reconstruct section presence");
    CHECK(copy.Played.Is == source.Played.Is && copy.Played.Starts == source.Played.Starts &&
              copy.Played.View == source.Played.View,
          "owned player selectors roundtrip XML characters");
    CHECK(copy.Played.EyeHeightM == source.Played.EyeHeightM && copy.Played.WalkMs == 0 &&
              copy.Played.RunMs == source.Played.RunMs,
          "player magnitudes roundtrip exactly");
  }
  for (const auto *field : {"eyeHeightM", "walkMs", "runMs"}) {
    for (const auto *value : {"nan", "inf", "1e3000", "", "1x", "-1"}) {
      const auto text =
          std::string("<scenario><player ") + field + "=\"" + value + "\"/></scenario>";
      Scenario::Document copy;
      copy.Played.Is = "preserved";
      std::string error;
      CHECK(!ReadScenario(text.data(), text.size(), copy, error),
            "malformed player value rejected");
      CHECK(copy.Played.Is == "preserved", "failure preserves prior player");
    }
  }
  for (const auto field : std::array{
           &Scenario::Player::EyeHeightM, &Scenario::Player::WalkMs, &Scenario::Player::RunMs}) {
    for (double value : {-1.0,
                         std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()}) {
      Scenario::Document invalid;
      invalid.Played.*field = value;
      CHECK(!WriteScenario(invalid), "writer rejects invalid player magnitude even if undeclared");
    }
  }
  const auto empty = WriteScenario({});
  CHECK(empty && empty->find("<player") == std::string::npos, "default absent player stays absent");
  return Report();
}
