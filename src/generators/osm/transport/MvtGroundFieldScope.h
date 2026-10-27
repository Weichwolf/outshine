#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTGROUNDFIELDSCOPE_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTGROUNDFIELDSCOPE_H

#include "GroundPortFieldScope.h"
#include "MvtGroundPortInputs.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>

namespace outshine::Generators::Osm {
struct MvtTransportSnapshot;

struct MvtGroundFieldLimits {
  size_t Nodes = 16384;
  size_t DirectedEdges = 32768;
  size_t Members = 32768;
  size_t Ways = 16384;
  size_t Omissions = 16384;
  size_t CoverageBoxes = 1024;
  size_t SelectedSegments = 2048;
  size_t MemberVisits = 65536;
  size_t ContainerBytes = 4194304;
};

struct MvtGroundFieldWork {
  size_t OriginalMemberVisits = 0;
  size_t QualifiedMemberVisits = 0;
  size_t ContentMemberVisits = 0;
  size_t CoverageTests = 0;
  size_t ContainerCapacityBytes = 0;
};

enum class MvtGroundFieldError : uint8_t {
  MissingFacts,
  InvalidBinding,
  InvalidCandidate,
  IncompleteIncidence,
  Frontier,
  MissingTopology,
  UnknownExit,
  MissingProfile,
  CoverageGap,
  CapacityExceeded,
  Canceled
};

struct MvtGroundFieldScope {
  GroundPortFieldScope Field;
  MvtGroundFieldWork Work;
};

[[nodiscard]] std::expected<MvtGroundFieldScope, MvtGroundFieldError>
BindMvtGroundFieldScope(const MvtTransportSnapshot &snapshot,
                        const MvtGroundFieldCandidateInputs &inputs,
                        GroundPortSourceChain candidate,
                        MvtGroundFieldLimits limits = {},
                        const std::stop_token &stop = {});
}
#endif
