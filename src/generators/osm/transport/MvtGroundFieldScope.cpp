#include "MvtGroundFieldScopeInternal.h"
#include "MvtTransportSnapshotInternal.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <utility>

namespace outshine::Generators::Osm {
namespace {
bool Fits(const MvtTransportSnapshot &snapshot,
          const GroundPortSourceChain &source,
          MvtGroundFieldLimits limits) {
  const auto &facts = *snapshot.SourceFacts;
  const auto &graph = snapshot.Network->Topology();
  if (graph.Nodes().size() > limits.Nodes || graph.Edges().size() > limits.DirectedEdges ||
      facts.Groups.size() > limits.Nodes || facts.Members.size() > limits.Members ||
      facts.Ways.size() > limits.Ways || snapshot.SourceWayCount > limits.Ways ||
      snapshot.Omissions.size() > limits.Omissions ||
      snapshot.Network->Coverage().size() > limits.CoverageBoxes ||
      source.SourceEdges.size() > limits.SelectedSegments ||
      facts.Members.size() > limits.MemberVisits) {
    return false;
  }
  size_t remaining = limits.ContainerBytes;
  const auto charge = [&](size_t count, size_t width) {
    if (count > remaining / width) { return false; }
    remaining -= count * width;
    return true;
  };
  return charge(graph.Nodes().size() + graph.Edges().size() + snapshot.SourceWayCount +
                    source.Nodes.size(),
                sizeof(uint32_t)) &&
         charge(source.Nodes.size(), sizeof(uint8_t)) &&
         charge(source.SourceEdges.size(), 2 * sizeof(uint8_t)) &&
         charge(snapshot.Network->Coverage().size(), 3 * sizeof(std::array<double, 2>)) &&
         charge(source.Nodes.capacity() + source.SourceEdges.capacity(), sizeof(uint32_t)) &&
         charge(snapshot.Network->SourceIdentity().Revision.size(), 2) && charge(256, 1);
}
}

std::expected<MvtGroundFieldScope, MvtGroundFieldError>
BindMvtGroundFieldScope(const MvtTransportSnapshot &snapshot,
                        const MvtGroundFieldCandidateInputs &inputs,
                        GroundPortSourceChain candidate,
                        MvtGroundFieldLimits limits,
                        const std::stop_token &stop) {
  using namespace GroundFieldScopes;
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (!snapshot.Network || !snapshot.SourceFacts || !snapshot.NodeGroups ||
      snapshot.SourceFactState != MvtTransportSourceFactState::Captured ||
      snapshot.NodeGroupState != MvtTransportNodeGroupState::Captured) {
    return std::unexpected(Error::MissingFacts);
  }
  const auto &facts = *snapshot.SourceFacts;
  if (snapshot.Network != inputs.Ports().Network || inputs.Facts() != snapshot.SourceFacts ||
      snapshot.Network != candidate.Source || facts.Network != snapshot.Network ||
      facts.NodeGroups != snapshot.NodeGroups ||
      facts.SourceGeneration != snapshot.SourceGeneration ||
      facts.SourceWayCount != snapshot.SourceWayCount ||
      facts.Groups.size() != facts.NodeGroups->Groups.size() ||
      snapshot.StreetWays.size() != snapshot.Network->Provenance().Paths.size() ||
      inputs.Ports().ProfileInput.DatasetId.empty() ||
      inputs.PathOmissions().size() != snapshot.Network->Provenance().Paths.size()) {
    return std::unexpected(Error::InvalidBinding);
  }
  if (!Fits(snapshot, candidate, limits)) { return std::unexpected(Error::CapacityExceeded); }
  if (candidate.SourceEdges.empty() || candidate.Nodes.size() != candidate.SourceEdges.size() + 1 ||
      !std::isfinite(candidate.WidthM) || candidate.WidthM <= 0 ||
      !std::isfinite(candidate.MaximumGradient) || candidate.MaximumGradient <= 0 ||
      candidate.MaximumGradient > 10) {
    return std::unexpected(Error::InvalidCandidate);
  }
  if (inputs.OmissionBinding() != MvtTransport::GroundInputOmissionDigest(snapshot.Omissions)) {
    return std::unexpected(Error::InvalidBinding);
  }
  State state{.Snapshot = snapshot,
              .Inputs = inputs,
              .Facts = facts,
              .Limits = limits,
              .Stop = stop,
              .Output = {.Field = {.Source = std::move(candidate), .Info = {}}, .Work = {}},
              .NodeOwners = {},
              .EdgeOwners = {},
              .Ways = {},
              .Groups = {},
              .Seen = {},
              .KnownUnknown = {}};
  if (auto indexed = Index(state); !indexed) { return std::unexpected(indexed.error()); }
  if (auto coverage = Coverage(state); !coverage) { return std::unexpected(coverage.error()); }
  if (auto qualified = Qualify(state); !qualified) { return std::unexpected(qualified.error()); }
  Bind(state);
  if (state.Output.Work.ContainerCapacityBytes > limits.ContainerBytes) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  return std::move(state.Output);
}
}
