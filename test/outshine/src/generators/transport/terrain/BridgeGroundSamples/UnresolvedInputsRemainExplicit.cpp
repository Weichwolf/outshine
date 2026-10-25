#include "BridgeGroundFixture.h"
#include "TransportCoreFixture.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stop_token>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

bool Gap(const BridgeGroundSamples &samples, size_t at, BridgeGroundGap reason) {
  const auto *gap = at < samples.Samples.size()
                        ? std::get_if<BridgeGroundGap>(&samples.Samples[at].Value)
                        : nullptr;
  return gap != nullptr && *gap == reason;
}

void CheckMissingData() {
  const auto fixture = MakeBridgeGroundFixture();
  if (!fixture.Terrain || !fixture.Inputs.Source) { return; }
  const auto missing = SampleBridgeGround(fixture.Inputs, {}, kGroundLimits);
  CHECK(missing && missing->Work.Sourced == 0 && missing->Work.Unresolved == 2 &&
            Gap(*missing, 0, BridgeGroundGap::MissingTerrain),
        "missing DEM remains a per-source pending input instead of a fabricated sea-level value");
  std::vector<Ground::HeightField::Block> blocks(fixture.Terrain->Blocks().begin(),
                                                 fixture.Terrain->Blocks().end());
  const int zoom = fixture.Terrain->Zoom();
  const auto partial = Ground::HeightField::Of(zoom, {blocks[0]});
  const auto one = SampleBridgeGround(fixture.Inputs, partial, kGroundLimits);
  CHECK(one && one->Work.Sourced == 1 && one->Work.Unresolved == 1 &&
            std::holds_alternative<BridgeGroundValue>(one->Samples[0].Value) &&
            Gap(*one, 1, BridgeGroundGap::MissingTerrain),
        "a missing distant landing does not discard another valid original ground condition");
  blocks[1].MissingBoundary = true;
  const auto boundary =
      SampleBridgeGround(fixture.Inputs, Ground::HeightField::Of(zoom, blocks), kGroundLimits);
  CHECK(boundary && boundary->Work.Sourced == 1 &&
            Gap(*boundary, 1, BridgeGroundGap::PartialBoundary),
        "partial boundary data cannot qualify a complete terrain condition");
  const auto fallback = SampleBridgeGround(
      fixture.Inputs, Ground::HeightField::Of(zoom, blocks, true), kGroundLimits);
  CHECK(fallback && fallback->Work.Sourced == 0 &&
            Gap(*fallback, 0, BridgeGroundGap::FallbackTerrain),
        "a fallback raster cannot be promoted by the presence of source metadata");
  blocks[1].MissingBoundary = false;
  blocks[1].Sources[0].Revision.clear();
  const auto unbound =
      SampleBridgeGround(fixture.Inputs, Ground::HeightField::Of(zoom, blocks), kGroundLimits);
  CHECK(unbound && unbound->Work.Sourced == 1 &&
            Gap(*unbound, 1, BridgeGroundGap::MissingProvenance),
        "a source name without a version cannot masquerade as a reproducible consumed DEM");
}

void CheckCoverageAndRasterErrors() {
  auto fixture = MakeBridgeGroundFixture();
  if (!fixture.Terrain || !fixture.Inputs.Source) { return; }
  std::vector<Ground::HeightField::Block> blocks(fixture.Terrain->Blocks().begin(),
                                                 fixture.Terrain->Blocks().end());
  auto unrelated = blocks[0];
  ++unrelated.At.Y;
  unrelated.MissingBoundary = true;
  blocks.push_back(unrelated);
  const auto extended = Ground::HeightField::Of(fixture.Terrain->Zoom(), blocks);
  const auto validSubset = SampleBridgeGround(fixture.Inputs, extended, kGroundLimits);
  CHECK(!extended->Qualified() && validSubset && validSubset->Work.Sourced == 2 &&
            validSubset->Work.Unresolved == 0,
        "unrequested partial DEM blocks do not poison the two actual consumed source conditions");
  blocks.resize(2);
  blocks[1].Terrain.reset();
  blocks[1].Nodes.assign(4, static_cast<float>(GroundSample::kTallestM + 1));
  const auto malformed = SampleBridgeGround(
      fixture.Inputs, Ground::HeightField::Of(fixture.Terrain->Zoom(), blocks), kGroundLimits);
  CHECK(malformed && malformed->Work.Sourced == 1 &&
            Gap(*malformed, 1, BridgeGroundGap::InvalidHeight),
        "invalid delivered elevation remains explicit instead of defaulting to zero");
  const auto *east = fixture.Inputs.Source->Topology().FindNode(2);
  if (east == nullptr) { return; }
  constexpr double northPole = 90;
  fixture.Inputs.Source =
      CoreSource({{.SourceNodeId = 1, .LatitudeDeg = northPole}, *east},
                 {{.Id = {.PathId = kCoreBridgePath}, .FromNodeId = 1, .ToNodeId = 2}});
  const auto outside = SampleBridgeGround(fixture.Inputs, fixture.Terrain, kGroundLimits);
  CHECK(outside && Gap(*outside, 0, BridgeGroundGap::OutsideCoverage) && outside->Work.Sourced == 1,
        "a polar query cannot be silently clamped to another latitude and called sourced DEM");
}

void CheckAdmission() {
  const auto fixture = MakeBridgeGroundFixture();
  auto limits = kGroundLimits;
  limits.ContainerCapacityMost = 2 * sizeof(BridgeGroundSample) - 1;
  BeginCppAllocationProbe();
  const auto denied = SampleBridgeGround(fixture.Inputs, fixture.Terrain, limits);
  const auto deniedAllocation = EndCppAllocationProbe();
  CHECK(!denied && denied.error().Code == BridgeGroundErrorCode::CapacityExceeded &&
            deniedAllocation.Calls == 0,
        "result byte admission is decided before allocating or reading any source samples");
  limits = kGroundLimits;
  limits.TerrainBlocksMost = 1;
  BeginCppAllocationProbe();
  const auto tooMany = SampleBridgeGround(fixture.Inputs, fixture.Terrain, limits);
  const auto tooManyAllocation = EndCppAllocationProbe();
  CHECK(!tooMany && tooMany.error().Code == BridgeGroundErrorCode::CapacityExceeded &&
            tooManyAllocation.Calls == 0,
        "the bounded source-block search is admitted independently of owned result bytes");
  std::stop_source stop;
  stop.request_stop();
  BeginCppAllocationProbe();
  const auto canceled =
      SampleBridgeGround(fixture.Inputs, fixture.Terrain, kGroundLimits, stop.get_token());
  const auto canceledAllocation = EndCppAllocationProbe();
  CHECK(!canceled && canceled.error().Code == BridgeGroundErrorCode::Canceled &&
            canceledAllocation.Calls == 0,
        "already canceled sampling retains no provisional result storage");
  auto bad = fixture.Inputs;
  bad.GroundNodes = {1, 1};
  const auto duplicate = SampleBridgeGround(bad, fixture.Terrain, kGroundLimits);
  CHECK(!duplicate && duplicate.error().Code == BridgeGroundErrorCode::InvalidSelection &&
            duplicate.error().Node == 1,
        "a repeated source-node request is not duplicate height work");
  bad.GroundNodes = {std::numeric_limits<uint64_t>::max()};
  const auto absent = SampleBridgeGround(bad, fixture.Terrain, kGroundLimits);
  CHECK(!absent && absent.error().Code == BridgeGroundErrorCode::InvalidSource &&
            absent.error().Node == bad.GroundNodes[0],
        "a wrong source-node binding remains a typed input error with its exact original ID");
  BeginCppAllocationProbe();
  const auto empty = SampleBridgeGround({}, {}, kGroundLimits);
  const auto emptyAllocation = EndCppAllocationProbe();
  CHECK(empty && empty->Samples.empty() && emptyAllocation.Calls == 0,
        "no ground demand performs no DEM or temporary result allocation");
}
}

int main() {
  CheckMissingData();
  CheckCoverageAndRasterErrors();
  CheckAdmission();
  return Report();
}
