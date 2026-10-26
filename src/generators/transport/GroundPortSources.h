#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUNDPORTSOURCES_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUNDPORTSOURCES_H

#include "TransportTopology.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::World {
class TransportNetworkSnapshot;
}

namespace outshine::Generators {
struct GroundPortSourceProfile {
  uint64_t PathId = 0;
  double MaximumGradient = 0;
};

struct GroundPortSourceLimits {
  size_t Nodes = 16384;
  size_t DirectedEdges = 16384;
  size_t PhysicalSegments = 8192;
  size_t CoverageBoxes = 1024;
  size_t ContainerBytes = 4194304;
};

struct GroundPortChainLimits {
  size_t SelectedSegments = 2048;
  size_t IncidenceVisits = 65536;
  size_t ContainerBytes = 1048576;
};

struct GroundPortSourceWork {
  size_t ValidatedNodes = 0;
  size_t ValidatedEdges = 0;
  size_t ValidatedOutgoing = 0;
  size_t DirectedEdgesVisited = 0;
  size_t PhysicalSegments = 0;
  size_t IndexNodeLookups = 0;
  size_t FrontierNodeLookups = 0;
  size_t ProfileLookups = 0;
  size_t CoverageTests = 0;
  size_t IncidenceEntries = 0;
  size_t ContainerCapacityBytes = 0;
};

struct GroundPortChainWork {
  size_t SelectedSegments = 0;
  size_t IncidenceVisits = 0;
  size_t NodesVisited = 0;
  size_t ProfileSegmentsChecked = 0;
  size_t OrderedSegments = 0;
  size_t ContainerCapacityBytes = 0;
};

enum class GroundPortSourceError : uint8_t {
  MissingSource,
  InvalidSource,
  InvalidProfile,
  InvalidFrontier,
  MissingSeed,
  UnknownFrontier,
  CapacityExceeded,
  Canceled,
  BridgeRecipeRequired,
  TunnelRecipeRequired,
  LayerRecipeRequired,
  MissingProfile,
  MixedProfiles,
  BranchedOrClosed,
  AmbiguousParallel
};

struct GroundPortSourceChain {
  std::shared_ptr<const World::TransportNetworkSnapshot> Source;
  std::vector<uint32_t> Nodes;
  std::vector<uint32_t> SourceEdges;
  double WidthM = 0;
  double MaximumGradient = 0;
  GroundPortChainWork Work;
};

class GroundPortSources {
public:
  GroundPortSources(GroundPortSources &&) = default;
  GroundPortSources &operator=(GroundPortSources &&) = default;
  GroundPortSources(const GroundPortSources &) = delete;
  GroundPortSources &operator=(const GroundPortSources &) = delete;

  [[nodiscard]] static std::expected<GroundPortSources, GroundPortSourceError>
  Make(std::shared_ptr<const World::TransportNetworkSnapshot> source,
       std::span<const GroundPortSourceProfile> profiles,
       std::span<const uint64_t> unknownNodes,
       GroundPortSourceLimits limits = {},
       const std::stop_token &stop = {});

  [[nodiscard]] std::expected<GroundPortSourceChain, GroundPortSourceError>
  Chain(World::TransportEdgeId seed,
        GroundPortChainLimits limits = {},
        const std::stop_token &stop = {}) const;

  [[nodiscard]] GroundPortSourceWork Work() const noexcept { return Work_; }

private:
  struct Segment {
    uint32_t Edge = 0;
    std::array<uint32_t, 2> Nodes{};
    double MaximumGradient = 0;
  };

  GroundPortSources() = default;

  struct Traversal {
    std::vector<uint8_t> Queued;
    std::vector<uint8_t> Seen;
    std::vector<uint32_t> Queue;
    GroundPortChainWork Work;
    bool Unknown = false;
  };

  [[nodiscard]] size_t CapacityBytes() const noexcept;
  [[nodiscard]] std::expected<void, GroundPortSourceError>
  Index(std::span<const GroundPortSourceProfile> profiles, const std::stop_token &stop);
  [[nodiscard]] std::expected<void, GroundPortSourceError>
  Frontier(std::span<const uint64_t> unknownNodes, const std::stop_token &stop);
  [[nodiscard]] std::expected<void, GroundPortSourceError>
  Order(std::span<const uint32_t> component, GroundPortSourceChain &result) const;
  [[nodiscard]] std::expected<uint32_t, GroundPortSourceError>
  Qualify(std::span<const uint32_t> component, GroundPortSourceChain &result) const;
  [[nodiscard]] std::expected<void, GroundPortSourceError>
  Expand(uint32_t node, Traversal &state, GroundPortChainLimits limits) const;

  std::shared_ptr<const World::TransportNetworkSnapshot> Source_;
  std::vector<Segment> Segments_;
  std::vector<uint32_t> Offsets_;
  std::vector<uint32_t> Incidence_;
  std::vector<uint8_t> Unknown_;
  GroundPortSourceWork Work_;
};
}
#endif
