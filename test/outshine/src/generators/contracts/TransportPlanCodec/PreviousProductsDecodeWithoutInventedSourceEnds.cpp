#include "TransportPlanFixture.h"
#include "ReadTextFile.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto original = ReadTextFile("test/outshine/data/generators/transport/plan-v3.bin",
                                     kTransportFixtureLimits.PackageBytesMost);
  CHECK(original.has_value(), "the fixed packet emitted by the original v3 producer is available");
  if (!original) { return Report(); }
  const auto bytes =
      std::span(reinterpret_cast<const uint8_t *>(original->data()), original->size());
  const auto decoded = DecodeTransportPlan(bytes, kTransportFixtureLimits);
  CHECK(decoded && decoded->SourceEnds.empty() && decoded->Coverage == TransportCoverage::Complete,
        "the previous recipe and wire format restore without inventing source-end records");
  const auto fixture = TransportPlanFixture();
  CHECK(fixture.has_value(), "the unchanged independently declared semantic fixture is available");
  if (!decoded || !fixture) { return Report(); }
  CHECK(EncodeTransportPlan(*decoded, kTransportFixtureLimits) ==
            EncodeTransportPlan(*fixture, kTransportFixtureLimits),
        "the original v3 packet and current fixture migrate to the same complete native product");
  auto mismatched = std::vector<uint8_t>(bytes.begin(), bytes.end());
  mismatched[sizeof(uint64_t)] = static_cast<uint8_t>(kTransportPlanRecipeVersion);
  CHECK(!DecodeTransportPlan(mismatched, kTransportFixtureLimits),
        "a new recipe number cannot relabel an old wire layout");
  return Report();
}
