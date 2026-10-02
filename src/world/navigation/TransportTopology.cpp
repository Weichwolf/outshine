#include "TransportTopology.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace outshine::World {

TransportTopology::TransportTopology(Data::SourceIdentity identity,
                                     std::vector<TransportNode> nodes,
                                     std::vector<TransportEdge> edges,
                                     size_t unclassifiedPathCount)
    : SourceIdentity_(std::move(identity)),
      Nodes_(std::move(nodes)),
      Edges_(std::move(edges)),
      UnclassifiedPathCount_(unclassifiedPathCount) {
  assert(Edges_.size() <= std::numeric_limits<uint32_t>::max());
  std::ranges::sort(Nodes_, {}, &TransportNode::SourceNodeId);
  std::ranges::sort(Edges_, {}, &TransportEdge::Id);
  Outgoing_.reserve(Edges_.size());
  for (size_t index = 0; index < Edges_.size(); ++index) {
    Outgoing_.push_back(OutgoingTransportEdge{.NodeId = Edges_[index].FromNodeId,
                                              .EdgeIndex = static_cast<uint32_t>(index)});
  }
  std::ranges::sort(Outgoing_,
                    [&](const OutgoingTransportEdge &left, const OutgoingTransportEdge &right) {
                      if (left.NodeId != right.NodeId) { return left.NodeId < right.NodeId; }
                      return Edges_[left.EdgeIndex].Id < Edges_[right.EdgeIndex].Id;
                    });
}

const TransportNode *TransportTopology::FindNode(uint64_t id) const noexcept {
  const auto found = std::ranges::lower_bound(Nodes_, id, {}, &TransportNode::SourceNodeId);
  return found != Nodes_.end() && found->SourceNodeId == id ? &*found : nullptr;
}

const TransportEdge *TransportTopology::FindEdge(TransportEdgeId id) const noexcept {
  const auto found = std::ranges::lower_bound(Edges_, id, {}, &TransportEdge::Id);
  return found != Edges_.end() && found->Id == id ? &*found : nullptr;
}

std::span<const OutgoingTransportEdge>
TransportTopology::OutgoingFrom(uint64_t nodeId) const noexcept {
  const auto first =
      std::ranges::lower_bound(Outgoing_, nodeId, {}, &OutgoingTransportEdge::NodeId);
  const auto last = std::ranges::upper_bound(Outgoing_, nodeId, {}, &OutgoingTransportEdge::NodeId);
  return {first, last};
}

bool TransportTopology::CanContinue(const TransportEdge &from, const TransportEdge &to) noexcept {
  return from.ToNodeId == to.FromNodeId && (from.Modes & to.Modes) != 0 &&
         from.Access != TransportAccess::Forbidden && to.Access != TransportAccess::Forbidden;
}

}
