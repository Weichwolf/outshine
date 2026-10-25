#include "Wayfinding.h"
#include "Earth.h"
#include <cstddef>
#include <optional>
#include <span>

namespace outshine::Path {
std::optional<LongitudeLatitude> Network::PositionOfNode(size_t node) const noexcept {
  if (!Woven_ || node >= Nodes_.size()) { return std::nullopt; }
  return LongitudeLatitude{.LongitudeDeg = Nodes_[node].LongitudeDeg,
                           .LatitudeDeg = Nodes_[node].LatitudeDeg};
}

std::span<const Network::Connection> Network::ConnectionsFrom(size_t node) const noexcept {
  if (!Woven_ || node >= Nodes_.size()) { return {}; }
  const auto &from = Nodes_[node];
  return std::span<const Connection>(Edges_).subspan(from.FirstEdge, from.EdgeCount);
}
}
