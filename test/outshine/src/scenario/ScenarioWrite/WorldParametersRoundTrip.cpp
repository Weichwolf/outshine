#include "ScenarioRead.h"
#include "ScenarioWrite.h"
#include "Check.h"
#include <array>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Scenario::Document source;
  source.Ground.Declared = true;
  source.Ground.Origin = {.LatitudeDeg = -47.12345678901234,
                          .LongitudeDeg = 181.1234567890123,
                          .RadiusM = 1234.56789012345};
  source.Ground.GravityMs2 = 1.23456789012345;
  source.Ground.AirDensityKgM3 = 0.123456789012345;
  source.Ground.PatienceS = 2.3456789012345;
  source.Ground.SightM = 23456.789012345;
  const auto text = WriteScenario(source);
  CHECK(text.has_value(), "world settings export succeeds");
  if (!text) { return Report(); }
  Scenario::Document copy;
  std::string error;
  CHECK(ReadScenario(text->data(), text->size(), copy, error), error.c_str());
  CHECK(copy.Ground.Origin.LatitudeDeg == source.Ground.Origin.LatitudeDeg &&
            copy.Ground.Origin.LongitudeDeg == source.Ground.Origin.LongitudeDeg &&
            copy.Ground.Origin.RadiusM == source.Ground.Origin.RadiusM,
        "geographic origin and generator extent survive exactly");
  for (const auto field : std::array{&Scenario::WorldSettings::GravityMs2,
                                     &Scenario::WorldSettings::AirDensityKgM3,
                                     &Scenario::WorldSettings::PatienceS,
                                     &Scenario::WorldSettings::SightM}) {
    CHECK(copy.Ground.*field == source.Ground.*field, "world parameter survives exactly");
    auto invalid = source;
    invalid.Ground.*field = -1;
    CHECK(!WriteScenario(invalid), "export rejects negative world magnitudes");
  }
  for (const auto *name :
       {"lat", "lon", "radiusM", "gravityMs2", "airDensityKgM3", "patienceS", "sightM"}) {
    for (const auto *value : {"nan", "inf", "1e3000", "", "0.5x"}) {
      const auto xml = std::string("<scenario><world ") + name + "=\"" + value + "\"/></scenario>";
      copy.Named.Name = "preserved";
      CHECK(!ReadScenario(xml.data(), xml.size(), copy, error), "bad world token refused");
      CHECK(copy.Named.Name == "preserved", "world parse failure preserves document");
    }
  }
  return Report();
}
