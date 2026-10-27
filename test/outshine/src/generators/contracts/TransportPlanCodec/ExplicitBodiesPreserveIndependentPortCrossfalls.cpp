#include "TransportPlanFixture.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void CheckBoundPortFields(const TransportPlan &original) {
  for (const unsigned field : {0U, 1U, 2U, 3U, 4U}) {
    auto changed = original;
    auto &port = changed.Ports[0];
    if (field == 0) { port.SourceId += "/foreign"; }
    if (field == 1) { ++port.Layer; }
    if (field == 2) { port.Use = TransportUse::Rail; }
    if (field == 3) { port.Profile = RoadProfile::Simple; }
    if (field == 4) { port.Contact.Gate.HalfWidthM *= 2; }
    const auto refused = EncodeTransportPlan(changed, kTransportFixtureLimits);
    CHECK(
        !refused && refused.error() == TransportPlanCodecError::InvalidPath,
        "explicit bodies preserve source, layer, use, profile and width contracts at their ports");
  }
}
}

int main() {
  const auto fixture = TransportPlanFixture();
  CHECK(fixture.has_value(), "the existing qualified native plan loads unchanged");
  if (!fixture) { return Report(); }
  auto plan = *fixture;
  const auto bounds = plan.Definition.Core.Bounds();
  const auto west = TransportBoundaryOf(plan.Definition.Core, TransportCoreSide::West);
  CHECK(bounds && west, "the second endpoint belongs to the actual other geographic boundary");
  if (!bounds || !west) { return Report(); }
  auto first = plan.Ports.front();
  first.Boundary = *west;
  first.Anchor.LongitudeDeg = bounds->WestDeg;
  first.Contact.Plane = {.SlopeE = .032149874557846327, .SlopeN = -.039908321037918505};
  first.CrossfallRad = .010382983774355937;
  plan.Ports.insert(plan.Ports.begin(), first);
  plan.Ports[1].Contact.Plane = {.SlopeE = -.0084848614190246741, .SlopeN = .042067964538463128};
  plan.Ports[1].CrossfallRad = .011656814717935211;
  plan.Paths[0].Ends = {TransportBoundaryPortReference{.Port = 0},
                        TransportBoundaryPortReference{.Port = 1}};
  const auto sweep = EncodeTransportPlan(plan, kTransportFixtureLimits);
  CHECK(!sweep && sweep.error() == TransportPlanCodecError::InvalidPath,
        "a constant sweep cannot silently absorb two different supplied port crossfalls");
  plan.Paths[0].Body = 0;
  const auto encoded = EncodeTransportPlan(plan, kTransportFixtureLimits);
  CHECK(encoded.has_value(),
        "an explicit native surface retains its independent port planes instead of inventing a "
        "common sweep crossfall");
  if (!encoded) { return Report(); }
  const auto decoded = DecodeTransportPlan(*encoded, kTransportFixtureLimits);
  CHECK(decoded && decoded->Paths[0].Body == plan.Paths[0].Body &&
            decoded->Ports[0].CrossfallRad == plan.Ports[0].CrossfallRad &&
            decoded->Ports[1].CrossfallRad == plan.Ports[1].CrossfallRad &&
            decoded->Ports[0].Contact.Plane.SlopeE == plan.Ports[0].Contact.Plane.SlopeE &&
            decoded->Ports[1].Contact.Plane.SlopeN == plan.Ports[1].Contact.Plane.SlopeN,
        "the actual codec preserves both distinct endpoint frames and the selected body exactly");
  CheckBoundPortFields(plan);
  auto invalid = plan;
  invalid.Paths[0].Body = std::numeric_limits<uint32_t>::max();
  CHECK(!EncodeTransportPlan(invalid, kTransportFixtureLimits),
        "an explicit geometry reference must resolve to an existing native body");
  invalid = plan;
  invalid.Ports[0].CrossfallRad = std::numeric_limits<double>::quiet_NaN();
  CHECK(!EncodeTransportPlan(invalid, kTransportFixtureLimits),
        "an explicit body does not authorize invalid or nonfinite port metadata");
  return Report();
}
