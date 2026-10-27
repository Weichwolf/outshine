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
    size_t unrepresented,
    std::array<uint8_t, 32> binding)
    : Ports_(std::move(ports)),
      Facts_(std::move(facts)),
      PathOmissions_(std::move(omissions)),
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
      facts.Ways.size() > limits.SourceWays) {
    return std::unexpected(Error::CapacityExceeded);
  }
  auto normalized = NormalizeMvtGroundPortInputs(snapshot, field, streets, limits, stop);
  if (!normalized) { return std::unexpected(normalized.error()); }
  if (facts.Digest != MvtTransport::SourceFactDigest(facts)) {
    return std::unexpected(Error::InvalidBinding);
  }
  return MvtGroundFieldCandidateInputs(std::move(normalized->Ports),
                                       snapshot.SourceFacts,
                                       std::move(normalized->PathOmissions),
                                       normalized->UnrepresentedIncidences,
                                       MvtTransport::GroundInputOmissionDigest(snapshot.Omissions));
}
}
