#include "TransportPlanFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include <chrono>
#include <cstddef>
#include <print>
#include <string>

namespace {
size_t OwnedBytes(const outshine::Generators::TransportPlan &plan) {
  using namespace outshine;
  using namespace outshine::Generators;
  const auto &junction = plan.Junctions[0];
  const auto &body = plan.Bodies[0];
  const auto &stamp = plan.TerrainContacts[0];
  return plan.Inputs.capacity() * sizeof(TransportInputReference) +
         plan.Paths.capacity() * sizeof(TransportPath) +
         plan.Stations.capacity() * sizeof(RoadStation) +
         plan.Junctions.capacity() * sizeof(TransportJunction) +
         plan.Ports.capacity() * sizeof(TransportPort) +
         plan.Bodies.capacity() * sizeof(HeightSolid) +
         plan.TerrainContacts.capacity() * sizeof(EarthworkStamp) +
         junction.Sources.capacity() * sizeof(std::string) +
         junction.Gates.capacity() * sizeof(RoadGate) +
         body.Points.capacity() * sizeof(body.Points[0]) +
         body.Faces.capacity() * sizeof(body.Faces[0]) +
         body.WallParents.capacity() * sizeof(body.WallParents[0]) +
         body.WallKinds.capacity() * sizeof(body.WallKinds[0]) + stamp.HeapBytes() +
         plan.Inputs[0].DatasetId.capacity() + 1;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  auto source = TransportPlanFixture();
  CHECK(source.has_value(), "the native product fixture is available");
  if (!source) { return Report(); }
  source->Inputs[0].DatasetId.assign(256, 'd');
  const auto binding =
      TransportPlanInputBinding(source->Inputs, kTransportFixtureLimits.PackageBytesMost);
  CHECK(binding.has_value(), "a source identity exceeding small-string storage is explicit");
  if (!binding) { return Report(); }
  source->InputBinding = *binding;
  const auto bytes = EncodeTransportPlan(*source, kTransportFixtureLimits);
  CHECK(bytes.has_value(), "the complete native product is serialized before allocation tracking");
  if (!bytes) { return Report(); }
  const auto begin = std::chrono::steady_clock::now();
  BeginCppAllocationProbe();
  const auto decoded = DecodeTransportPlan(*bytes, kTransportFixtureLimits);
  const auto allocations = EndCppAllocationProbe();
  const auto duration = std::chrono::steady_clock::now() - begin;
  CHECK(decoded.has_value(), "the owned product decodes under its admission");
  if (!decoded) { return Report(); }
  constexpr size_t ownedBlocks = 18;
  const size_t owned = OwnedBytes(*decoded);
  std::println("TRANSPORT_PLAN_DECODE package_bytes={} cpp_allocations={} requested_bytes={} "
               "owned_bytes={} capacity_bytes={} wall_ns={}",
               bytes->size(),
               allocations.Calls,
               allocations.Bytes,
               owned,
               TransportPlanCapacityBytes(*decoded),
               std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
  CHECK(allocations.Calls == ownedBlocks && allocations.Bytes == owned,
        "each owned container/string allocates once without temporary decoded copies or scratch");
  BeginCppAllocationProbe();
  const auto refused = DecodeTransportPlan(
      *bytes, {.PackageBytesMost = bytes->size(), .DecodedCapacityMost = sizeof(TransportPlan)});
  const auto rejected = EndCppAllocationProbe();
  CHECK(!refused && refused.error() == TransportPlanCodecError::CapacityExceeded &&
            rejected.Calls == 0 && rejected.Bytes == 0,
        "rejecting the first container's admission allocates no product storage");
  return Report();
}
