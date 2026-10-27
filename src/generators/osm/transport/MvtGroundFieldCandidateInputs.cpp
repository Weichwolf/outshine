#include "MvtGroundPortInputsInternal.h"
#include "MvtTransportSnapshotInternal.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
MvtGroundFieldCandidateInputs::MvtGroundFieldCandidateInputs(
    MvtGroundPortInputs ports,
    std::shared_ptr<const MvtTransportSourceFacts> facts,
    std::vector<uint8_t> omissions,
    std::vector<uint32_t> groups,
    std::vector<uint32_t> ways,
    size_t unrepresented,
    std::array<uint8_t, 32> binding)
    : Ports_(std::move(ports)),
      Facts_(std::move(facts)),
      PathOmissions_(std::move(omissions)),
      Groups_(std::move(groups)),
      Ways_(std::move(ways)),
      UnrepresentedIncidences_(unrepresented),
      OmissionBinding_(binding) {}

std::expected<MvtGroundFieldCandidateInputs, MvtGroundPortInputError>
BindMvtGroundFieldCandidateInputs(const MvtTransportSnapshot &snapshot,
                                  const OsmField &field,
                                  const StreetField &streets,
                                  MvtGroundPortInputLimits limits,
                                  const std::stop_token &stop) {
  using Error = MvtGroundPortInputError;
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (!snapshot.Network) { return std::unexpected(Error::MissingSource); }
  if (!snapshot.SourceFacts || !snapshot.NodeGroups ||
      snapshot.SourceFactState != MvtTransportSourceFactState::Captured ||
      snapshot.NodeGroupState != MvtTransportNodeGroupState::Captured) {
    return std::unexpected(Error::MissingFacts);
  }
  const auto &facts = *snapshot.SourceFacts;
  if (facts.Network != snapshot.Network || facts.NodeGroups != snapshot.NodeGroups ||
      facts.SourceGeneration != snapshot.SourceGeneration ||
      facts.SourceWayCount != snapshot.SourceWayCount) {
    return std::unexpected(Error::InvalidBinding);
  }
  if (facts.Groups.size() > limits.Nodes || facts.Members.size() > limits.OriginalMembers ||
      facts.Ways.size() > limits.SourceWays ||
      snapshot.Network->Topology().Nodes().size() > limits.Nodes ||
      snapshot.SourceWayCount > limits.SourceWays ||
      facts.Ways.size() >= MvtGroundFieldCandidateInputs::MissingGroup ||
      facts.Groups.size() >= MvtGroundFieldCandidateInputs::AmbiguousGroup ||
      facts.NodeGroups->Nodes.size() > limits.Nodes) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (facts.Groups.size() != facts.NodeGroups->Groups.size()) {
    return std::unexpected(Error::InvalidBinding);
  }
  const auto nodes = snapshot.Network->Topology().Nodes().size();
  if (nodes > limits.ContainerBytes / sizeof(uint32_t) ||
      snapshot.SourceWayCount > limits.ContainerBytes / sizeof(uint32_t) - nodes) {
    return std::unexpected(Error::CapacityExceeded);
  }
  const auto indexBytes = (nodes + snapshot.SourceWayCount) * sizeof(uint32_t);
  auto normalizationLimits = limits;
  normalizationLimits.ContainerBytes -= indexBytes;
  auto normalized =
      NormalizeMvtGroundPortInputs(snapshot, field, streets, normalizationLimits, stop);
  if (!normalized) { return std::unexpected(normalized.error()); }
  if (facts.Digest != MvtTransport::SourceFactDigest(facts)) {
    return std::unexpected(Error::InvalidBinding);
  }
  auto index = IndexMvtGroundFieldFacts(facts, static_cast<size_t>(snapshot.SourceWayCount), stop);
  if (!index) { return std::unexpected(index.error()); }
  const auto capacity = (index->Groups.capacity() + index->Ways.capacity()) * sizeof(uint32_t);
  if (capacity > limits.ContainerBytes - normalized->Ports.Work.ContainerCapacityBytes) {
    return std::unexpected(Error::CapacityExceeded);
  }
  normalized->Ports.Work.ContainerCapacityBytes += capacity;
  normalized->Ports.Work.OriginalMemberVisits = index->MemberVisits;
  return MvtGroundFieldCandidateInputs(std::move(normalized->Ports),
                                       snapshot.SourceFacts,
                                       std::move(normalized->PathOmissions),
                                       std::move(index->Groups),
                                       std::move(index->Ways),
                                       normalized->UnrepresentedIncidences,
                                       MvtTransport::GroundInputOmissionDigest(snapshot.Omissions));
}
}
