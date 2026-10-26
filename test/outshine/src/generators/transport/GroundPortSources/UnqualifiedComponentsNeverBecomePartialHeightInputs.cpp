#include "Check.h"
#include "CppAllocationProbe.h"
#include "GroundPortSources.h"
#include "TransportCoreFixture.h"
#include "TransportNetworkSnapshot.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr std::array<GroundPortSourceProfile, 4> kProfiles{{{.PathId = 10, .MaximumGradient = .1},
                                                            {.PathId = 20, .MaximumGradient = .1},
                                                            {.PathId = 30, .MaximumGradient = .1},
                                                            {.PathId = 40, .MaximumGradient = .1}}};
constexpr World::TransportEdgeId kSeed{.PathId = 20};
constexpr GeographicBounds kCoverage{.WestDeg = -1, .SouthDeg = -1, .EastDeg = 1, .NorthDeg = 1};

[[nodiscard]] std::vector<World::TransportEdge> Edges() {
  using namespace World;
  return {{.Id = {.PathId = 10}, .FromNodeId = 1, .ToNodeId = 2, .WidthM = 3},
          {.Id = {.PathId = 10, .Direction = EdgeDirection::Reverse},
           .FromNodeId = 2,
           .ToNodeId = 1,
           .WidthM = 3},
          {.Id = {.PathId = 20, .Direction = EdgeDirection::Reverse},
           .FromNodeId = 3,
           .ToNodeId = 2,
           .WidthM = 3},
          {.Id = {.PathId = 30}, .FromNodeId = 3, .ToNodeId = 4, .WidthM = 3}};
}

[[nodiscard]] std::shared_ptr<const World::TransportNetworkSnapshot>
Source(std::vector<World::TransportEdge> edges) {
  std::vector<World::TransportNode> nodes;
  for (uint64_t at = 1; at <= 5; ++at) {
    nodes.push_back(
        {.SourceNodeId = at, .LatitudeDeg = 0, .LongitudeDeg = static_cast<double>(at) / 1000});
  }
  return CoreSource(std::move(nodes), std::move(edges), {kCoverage});
}

void CheckCanonical() {
  auto source = Source(Edges());
  const auto *original = source.get();
  const auto prepared = GroundPortSources::Make(source, kProfiles, {});
  CHECK(prepared.has_value(), "a directed source with reverse-only middle span is valid");
  if (!prepared) { return; }
  source.reset();
  const auto chain = prepared->Chain(kSeed);
  CHECK(chain && chain->Source.get() == original &&
            chain->Nodes == std::vector<uint32_t>({0, 1, 2, 3}) &&
            chain->SourceEdges == std::vector<uint32_t>({0, 2, 3}) && chain->WidthM == 3 &&
            chain->MaximumGradient == .1,
        "source ownership and canonical full-chain order survive the caller's lease release");
  CHECK(prepared->Work().PhysicalSegments == 3 && prepared->Work().IncidenceEntries == 6,
        "two directed aliases count as one physical incidence while reverse-only spans remain");
  const auto capped = prepared->Chain(kSeed, {.IncidenceVisits = 5});
  CHECK(!capped && capped.error() == GroundPortSourceError::CapacityExceeded,
        "a whole six-incidence chain cannot be qualified by five visits");
  BeginCppAllocationProbe();
  const auto refused = prepared->Chain(kSeed, {.ContainerBytes = 0});
  const auto allocations = EndCppAllocationProbe();
  CHECK(!refused && refused.error() == GroundPortSourceError::CapacityExceeded &&
            allocations.Calls == 0,
        "insufficient traversal storage is refused before allocating scratch or output");
  auto missing = kSeed;
  missing.PathId = 99;
  CHECK(!prepared->Chain(missing), "a missing physical seed cannot borrow another source segment");
}

void CheckGeometryRecipes() {
  const auto expect = [](std::vector<World::TransportEdge> edges, GroundPortSourceError reason) {
    const auto prepared = GroundPortSources::Make(Source(std::move(edges)), kProfiles, {});
    CHECK(prepared.has_value(), "unsupported height recipes retain their normalized source graph");
    if (!prepared) { return; }
    const auto chain = prepared->Chain(kSeed);
    CHECK(!chain && chain.error() == reason,
          "unsupported full components remain explicit pending height input");
  };
  auto edges = Edges();
  edges[2].Bridge = true;
  expect(edges, GroundPortSourceError::BridgeRecipeRequired);
  edges = Edges();
  edges[2].Tunnel = true;
  expect(edges, GroundPortSourceError::TunnelRecipeRequired);
  edges = Edges();
  edges[2].Layer = 1;
  expect(edges, GroundPortSourceError::LayerRecipeRequired);
  edges = Edges();
  edges[3].WidthM = 4;
  expect(edges, GroundPortSourceError::MixedProfiles);
  edges[3].WidthM = 0;
  expect(edges, GroundPortSourceError::MissingProfile);
  edges = Edges();
  edges.push_back({.Id = {.PathId = 40}, .FromNodeId = 2, .ToNodeId = 5, .WidthM = 3});
  expect(edges, GroundPortSourceError::BranchedOrClosed);
  edges = Edges();
  edges.push_back({.Id = {.PathId = 40}, .FromNodeId = 4, .ToNodeId = 1, .WidthM = 3});
  expect(edges, GroundPortSourceError::BranchedOrClosed);
}

void CheckFrontierAndAdmission() {
  const auto source = Source(Edges());
  constexpr std::array<uint64_t, 1> frontier{2};
  const auto prepared = GroundPortSources::Make(source, kProfiles, frontier);
  CHECK(prepared && !prepared->Chain(kSeed) &&
            prepared->Chain(kSeed).error() == GroundPortSourceError::UnknownFrontier,
        "an explicit unknown source connection forbids a numerical ground component");
  constexpr std::array<uint64_t, 1> absent{99};
  CHECK(!GroundPortSources::Make(source, kProfiles, absent),
        "frontiers must name original nodes instead of invented anchors");
  BeginCppAllocationProbe();
  const auto refused = GroundPortSources::Make(source, kProfiles, {}, {.ContainerBytes = 0});
  const auto allocations = EndCppAllocationProbe();
  CHECK(!refused && refused.error() == GroundPortSourceError::CapacityExceeded &&
            allocations.Calls == 0,
        "source-index admission precedes all container allocation");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = GroundPortSources::Make(source, kProfiles, {}, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == GroundPortSourceError::Canceled,
        "source preparation honors cancellation before indexing");
  CHECK(prepared && !prepared->Chain(kSeed, {}, stop.get_token()),
        "a canceled prepared traversal does not publish a chain");
  const auto missingProfile = GroundPortSources::Make(source, std::span(kProfiles).first(1), {});
  CHECK(missingProfile && !missingProfile->Chain(kSeed) &&
            missingProfile->Chain(kSeed).error() == GroundPortSourceError::MissingProfile,
        "a source without its qualified recipe cannot gain a guessed maximum gradient");
  auto edges = Edges();
  edges[1].WidthM = 4;
  CHECK(!GroundPortSources::Make(Source(edges), kProfiles, {}),
        "directed aliases cannot disagree about physical width");
  edges = Edges();
  edges[2].ToNodeId = edges[2].FromNodeId;
  CHECK(!GroundPortSources::Make(Source(edges), kProfiles, {}),
        "collapsed physical segments never enter incidence traversal");
  auto profiles = kProfiles;
  profiles[1].PathId = profiles[0].PathId;
  const auto duplicate = GroundPortSources::Make(source, profiles, {});
  CHECK(!duplicate && duplicate.error() == GroundPortSourceError::InvalidProfile,
        "ambiguous source profile bindings are rejected instead of selecting by insertion order");
  profiles = kProfiles;
  profiles[0].MaximumGradient = 11;
  CHECK(!GroundPortSources::Make(source, profiles, {}),
        "source gradient recipes retain the reference's finite numerical domain");
  const auto uncovered =
      CoreSource(std::vector<World::TransportNode>{{.SourceNodeId = 1}, {.SourceNodeId = 2}},
                 std::vector<World::TransportEdge>{
                     {.Id = {.PathId = 10}, .FromNodeId = 1, .ToNodeId = 2, .WidthM = 3}});
  const auto unknownCoverage = GroundPortSources::Make(uncovered, kProfiles, {});
  CHECK(unknownCoverage && !unknownCoverage->Chain({.PathId = 10}) &&
            unknownCoverage->Chain({.PathId = 10}).error() ==
                GroundPortSourceError::UnknownFrontier,
        "missing source coverage cannot be interpreted as a closed known world");
}
}

int main() {
  CheckCanonical();
  CheckGeometryRecipes();
  CheckFrontierAndAdmission();
  return Report();
}
