#include "Triggers.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array events{Scenario::Event{.Name = "dwelled"}};
  const std::array volumes{Scenario::Volume{
      .Shape = "box", .ExtentM = {1, 1, 1}, .Fires = "dwelled", .When = "dwell", .DwellS = 2}};
  auto field = TriggerField::Stand(volumes, events);
  CHECK(field.has_value(), "dwell field prepared");
  if (!field) { return Report(); }
  const Entity body{.Index = 1, .Generation = 1};
  CHECK(field->Probe(body, {}, 1).has_value(), "occupancy begins at time one");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  for (double bad : {nan, infinity, -1.0, 0.5}) {
    const auto result = field->Probe(body, {10, 0, 0}, bad);
    CHECK(!result && result.error() == TriggerField::ProbeError::InvalidTime,
          "invalid time cannot remove an occupant");
  }
  for (int axis = 0; axis < 3; ++axis) {
    for (double bad : {nan, infinity, -infinity}) {
      Vec3 position;
      position[axis] = bad;
      const auto result = field->Probe(body, position, 100);
      CHECK(!result && result.error() == TriggerField::ProbeError::InvalidPosition,
            "invalid position cannot change occupancy or advance the probe clock");
    }
  }
  const auto invalid = field->Probe(kNoEntity, {}, 100);
  CHECK(!invalid && invalid.error() == TriggerField::ProbeError::InvalidEntity,
        "sentinel is not an occupant");
  CHECK(field->Drain().empty(), "rejected probes emit nothing");
  CHECK(field->Probe(body, {}, 3).has_value(), "rejected probes preserve the last accepted time");
  CHECK(field->Drain().size() == 1, "original dwell start survives rejected outside probes");
  CHECK(field->Probe(body, {}, 3).has_value(), "equal timestamps remain valid");
  CHECK(field->Probe(body, {}, 4).has_value(), "continued dwell accepted");
  CHECK(field->Drain().empty(), "dwell fires only once per occupancy");
  CHECK(field->Probe(body, {2, 0, 0}, 5).has_value(), "leaving ends occupancy");
  CHECK(field->Probe(body, {}, 6).has_value(), "reentry starts new dwell");
  CHECK(field->Probe(body, {}, 7).has_value(), "new dwell remains below threshold");
  CHECK(field->Drain().empty(), "previous dwell does not carry through exit");
  CHECK(field->Probe(body, {}, 8).has_value(), "new dwell reaches threshold");
  CHECK(field->Drain().size() == 1, "reentry permits exactly one new dwell event");
  return Report();
}
