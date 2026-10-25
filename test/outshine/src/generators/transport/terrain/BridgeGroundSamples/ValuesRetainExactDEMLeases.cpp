#include "BridgeGroundFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <print>
#include <utility>
#include <variant>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  auto fixture = MakeBridgeGroundFixture();
  CHECK(fixture.Inputs.Source && fixture.Terrain && fixture.Revisions,
        "the selected native bridge and actual sourced terrain deliveries are available");
  if (!fixture.Inputs.Source || !fixture.Terrain || !fixture.Revisions) { return Report(); }
  const auto *source = fixture.Inputs.Source.get();
  const auto *terrain = fixture.Terrain.get();
  const auto *westNodes = fixture.Terrain->Blocks()[0].Terrain->Heights().data();
  BeginCppAllocationProbe();
  const auto sampled = SampleBridgeGround(fixture.Inputs, fixture.Terrain, kGroundLimits);
  const auto allocation = EndCppAllocationProbe();
  CHECK(sampled && sampled->Samples.size() == 2 && sampled->Source.get() == source &&
            sampled->Terrain.get() == terrain && sampled->Work.Sourced == 2 &&
            sampled->Work.Unresolved == 0 && allocation.Calls == 1 &&
            allocation.Bytes == 2 * sizeof(BridgeGroundSample) &&
            allocation.Bytes == sampled->Work.ContainerCapacityBytes,
        "only the two owned sample records allocate while source graph and DEM stay pinned "
        "without copying");
  if (!sampled || sampled->Samples.size() != 2) { return Report(); }
  const auto *west = std::get_if<BridgeGroundValue>(&sampled->Samples[0].Value);
  const auto *east = std::get_if<BridgeGroundValue>(&sampled->Samples[1].Value);
  CHECK(west != nullptr && east != nullptr && sampled->Samples[0].SourceNode == 1 &&
            sampled->Samples[1].SourceNode == 2 && west->AslM == kWestGroundAslM &&
            east->AslM == kEastGroundAslM && west->TerrainBlock == 0 && east->TerrainBlock == 1,
        "actual sourced zero and distant nonzero elevations retain their original node and "
        "raster-block identities");
  if (west == nullptr || east == nullptr) { return Report(); }
  const auto &block = sampled->Terrain->Blocks()[west->TerrainBlock];
  CHECK(block.Terrain->Heights().data() == westNodes && block.Nodes.empty() &&
            block.Sources.size() == 1 && block.Sources[0].SourceId == "bridge-dem" &&
            block.Sources[0].Revision == "r1" && block.Certificate.IsComplete() &&
            block.Certificate.TerrainScopeRevision() == kBridgeTerrainScope &&
            fixture.Revisions->AreCurrent(block.Certificate.Dependencies()),
        "provenance, registration/delivery certificates and original shared DEM storage remain "
        "attached to the actual sampled block");
  std::vector<Ground::HeightField::Block> reversed(fixture.Terrain->Blocks().begin(),
                                                   fixture.Terrain->Blocks().end());
  std::ranges::reverse(reversed);
  const auto reordered = Ground::HeightField::Of(fixture.Terrain->Zoom(), std::move(reversed));
  const auto other = SampleBridgeGround(fixture.Inputs, reordered, kGroundLimits);
  CHECK(other && other->Samples.size() == 2, "a differently ordered input snapshot is usable");
  if (other && other->Samples.size() == 2) {
    const auto *same = std::get_if<BridgeGroundValue>(&other->Samples[0].Value);
    CHECK(same != nullptr && same->AslM == west->AslM && same->TerrainBlock == 1 &&
              other->Terrain->Blocks()[same->TerrainBlock].Sources == block.Sources,
          "input block order changes the borrowed index while preserving the actual source "
          "condition and its consumed provenance");
  }
  fixture.Inputs.Source.reset();
  fixture.Terrain.reset();
  CHECK(sampled->Source.get() == source && sampled->Terrain.get() == terrain &&
            sampled->Terrain->Blocks()[west->TerrainBlock].Terrain->Heights().data() == westNodes,
        "releasing the upstream holders cannot release captured input data before planning ends");
  std::println("BRIDGE_GROUND_SAMPLES samples={} cpp_allocations={} requested_bytes={} "
               "node_lookups={} tile_classifications={} height_lookups={} source_checks={} "
               "sourced={} unresolved={}",
               sampled->Samples.size(),
               allocation.Calls,
               allocation.Bytes,
               sampled->Work.NodeLookups,
               sampled->Work.TileClassifications,
               sampled->Work.HeightLookups,
               sampled->Work.SourceReferenceChecks,
               sampled->Work.Sourced,
               sampled->Work.Unresolved);
  return Report();
}
