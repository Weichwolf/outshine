#ifndef OUTSHINE_GENERATORS_TRANSPORT_BRIDGEINPUTREQUIREMENTS_H
#define OUTSHINE_GENERATORS_TRANSPORT_BRIDGEINPUTREQUIREMENTS_H

#include "TransportTopology.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::World {
class TransportNetworkSnapshot;
}

namespace outshine::Generators {
enum class BridgeConditionReason : uint8_t { NoGroundConnection, NoGroundAnchor };

struct BridgeConditionRequest {
  uint64_t Node = 0;
  BridgeConditionReason Reason = BridgeConditionReason::NoGroundConnection;
  [[nodiscard]] bool operator==(const BridgeConditionRequest &) const = default;
};

struct BridgeInputWork {
  size_t SourceEdges = 0;
  size_t SeedLookups = 0;
  size_t PhysicalSegments = 0;
  size_t NodeLookups = 0;
  size_t BridgeSegments = 0;
  size_t AdjacencyEntries = 0;
  size_t BridgeVisits = 0;
  size_t AdjacencyVisits = 0;
  size_t PeakContainerCapacityBytes = 0;
};

struct BridgeInputRequirements {
  std::shared_ptr<const World::TransportNetworkSnapshot> Source;
  std::vector<uint32_t> BridgeEdges;
  std::vector<uint64_t> GroundNodes;
  std::vector<BridgeConditionRequest> Unqualified;
  BridgeInputWork Work;
};

struct BridgeInputLimits {
  size_t SourceEdgesMost = 0;
  size_t ContainerCapacityMost = 0;
};

enum class BridgeInputErrorCode : uint8_t {
  MissingSource,
  MissingSeed,
  InvalidSeed,
  InvalidSource,
  CapacityExceeded,
  Canceled
};

struct BridgeInputError {
  BridgeInputErrorCode Code;
  std::optional<World::TransportEdgeId> Edge;
  std::optional<uint64_t> Node;
};

[[nodiscard]] std::expected<BridgeInputRequirements, BridgeInputError>
SelectBridgeInputRequirements(std::shared_ptr<const World::TransportNetworkSnapshot> source,
                              std::span<const World::TransportEdgeId> seeds,
                              BridgeInputLimits limits,
                              const std::stop_token &stop = {});
}
#endif
