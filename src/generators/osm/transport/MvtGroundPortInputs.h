#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTGROUNDPORTINPUTS_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTGROUNDPORTINPUTS_H

#include "GroundPortSourceProfile.h"
#include "TransportPlan.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <limits>
#include <stop_token>
#include <span>
#include <vector>

namespace outshine::World {
class TransportNetworkSnapshot;
}

namespace outshine::Generators::Osm {
class OsmField;
class StreetField;
struct MvtTransportSnapshot;
struct MvtTransportSourceFacts;

struct MvtGroundPortInputLimits {
  size_t Nodes = 16384;
  size_t DirectedEdges = 32768;
  size_t Paths = 16384;
  size_t SourceWays = 16384;
  size_t OriginalMembers = 65536;
  size_t SourceTiles = 1024;
  size_t Omissions = 16384;
  size_t AttributeWords = 1048576;
  size_t ContainerBytes = 4194304;
};

struct MvtGroundPortInputWork {
  size_t SourceTiles = 0;
  size_t Paths = 0;
  size_t DirectedEdges = 0;
  size_t NodeLookups = 0;
  size_t AttributeWords = 0;
  size_t MissingProfiles = 0;
  size_t OmissionLookups = 0;
  size_t OriginalMemberVisits = 0;
  size_t ContainerCapacityBytes = 0;
};

enum class MvtGroundPortInputError : uint8_t {
  MissingSource,
  SourceChanged,
  InvalidBinding,
  MissingIncidence,
  CapacityExceeded,
  Canceled,
  MissingFacts
};

struct MvtGroundPortInputs {
  std::shared_ptr<const World::TransportNetworkSnapshot> Network;
  std::vector<GroundPortSourceProfile> Profiles;
  std::vector<uint64_t> UnknownNodes;
  TransportInputReference ProfileInput;
  MvtGroundPortInputWork Work;
};

class MvtGroundFieldCandidateInputs {
public:
  static constexpr uint32_t MissingGroup = std::numeric_limits<uint32_t>::max();
  static constexpr uint32_t AmbiguousGroup = MissingGroup - 1;
  MvtGroundFieldCandidateInputs() = default;

  [[nodiscard]] const MvtGroundPortInputs &Ports() const noexcept { return Ports_; }

  [[nodiscard]] const std::shared_ptr<const MvtTransportSourceFacts> &Facts() const noexcept {
    return Facts_;
  }

  [[nodiscard]] std::span<const uint8_t> PathOmissions() const noexcept { return PathOmissions_; }

  [[nodiscard]] size_t UnrepresentedIncidences() const noexcept { return UnrepresentedIncidences_; }

  [[nodiscard]] const std::array<uint8_t, 32> &OmissionBinding() const noexcept {
    return OmissionBinding_;
  }

  [[nodiscard]] std::span<const uint32_t> OriginalGroups() const noexcept { return Groups_; }

  [[nodiscard]] std::span<const uint32_t> WayFacts() const noexcept { return Ways_; }

private:
  MvtGroundFieldCandidateInputs(MvtGroundPortInputs ports,
                                std::shared_ptr<const MvtTransportSourceFacts> facts,
                                std::vector<uint8_t> omissions,
                                std::vector<uint32_t> groups,
                                std::vector<uint32_t> ways,
                                size_t unrepresented,
                                std::array<uint8_t, 32> binding);
  friend std::expected<MvtGroundFieldCandidateInputs, MvtGroundPortInputError>
  BindMvtGroundFieldCandidateInputs(const MvtTransportSnapshot &snapshot,
                                    const OsmField &field,
                                    const StreetField &streets,
                                    MvtGroundPortInputLimits limits,
                                    const std::stop_token &stop);
  MvtGroundPortInputs Ports_;
  std::shared_ptr<const MvtTransportSourceFacts> Facts_;
  std::vector<uint8_t> PathOmissions_;
  std::vector<uint32_t> Groups_, Ways_;
  size_t UnrepresentedIncidences_ = 0;
  std::array<uint8_t, 32> OmissionBinding_{};
};

[[nodiscard]] std::expected<MvtGroundFieldCandidateInputs, MvtGroundPortInputError>
BindMvtGroundFieldCandidateInputs(const MvtTransportSnapshot &snapshot,
                                  const OsmField &field,
                                  const StreetField &streets,
                                  MvtGroundPortInputLimits limits = {},
                                  const std::stop_token &stop = {});

[[nodiscard]] std::expected<MvtGroundPortInputs, MvtGroundPortInputError>
BindMvtGroundPortInputs(const MvtTransportSnapshot &snapshot,
                        const OsmField &field,
                        const StreetField &streets,
                        MvtGroundPortInputLimits limits = {},
                        const std::stop_token &stop = {});
}
#endif
