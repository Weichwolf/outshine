#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTTRANSPORTNODEGROUPS_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTTRANSPORTNODEGROUPS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Generators::Osm {

enum class MvtTransportNodeGroupState : uint8_t { NotRequested, Captured, BudgetExceeded };

struct MvtTransportCapture {
  size_t NodeGroupPayloadBytesMost = 0;
};

struct MvtTransportNodeGroup {
  uint32_t FirstNode = 0;
  uint32_t NodeCount = 0;
  uint64_t SourceConnectionCount = 0;
};

// This partitions published domain nodes, without qualifying connections between them.
struct MvtTransportNodeGroups {
  std::vector<MvtTransportNodeGroup> Groups;
  std::vector<uint64_t> Nodes;
  std::array<uint8_t, 32> Digest{};

  [[nodiscard]] std::span<const uint64_t> NodesOf(const MvtTransportNodeGroup &group) const {
    return std::span(Nodes).subspan(group.FirstNode, group.NodeCount);
  }
};

}
#endif
