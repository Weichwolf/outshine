#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTGROUNDFIELDSCOPEINTERNAL_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTGROUNDFIELDSCOPEINTERNAL_H

#include "MvtGroundFieldScope.h"
#include "MvtTransportSnapshot.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators::Osm::GroundFieldScopes {
using Error = MvtGroundFieldError;
template <typename T> using Checked = std::expected<T, Error>;
inline constexpr uint32_t kAbsent = std::numeric_limits<uint32_t>::max();

struct State {
  const MvtTransportSnapshot &Snapshot;
  const MvtGroundFieldCandidateInputs &Inputs;
  const MvtTransportSourceFacts &Facts;
  const MvtGroundFieldLimits &Limits;
  const std::stop_token &Stop;
  MvtGroundFieldScope Output;
  std::vector<uint32_t> NodeOwners, EdgeOwners;
  std::span<const uint32_t> Ways;
  std::vector<uint32_t> Groups;
  std::vector<uint8_t> Seen;
  std::vector<uint8_t> KnownUnknown;

  [[nodiscard]] const GroundPortSourceChain &Source() const noexcept { return Output.Field.Source; }
};

[[nodiscard]] Checked<void> Index(State &state);
[[nodiscard]] Checked<void> Qualify(State &state);
[[nodiscard]] Checked<void> AppendEnds(State &state,
                                       std::span<const TransportSourceMember> members,
                                       std::span<const uint32_t> grounds);
[[nodiscard]] Checked<void> Coverage(State &state);
void Bind(State &state);
[[nodiscard]] Checked<void> BindContent(State &state);
}
#endif
