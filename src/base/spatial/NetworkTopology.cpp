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

std::span<const double> Network::CoordinatesOfWay(size_t way) const noexcept {
  if (!Woven_ || way >= Ways_.size()) { return {}; }
  const auto &source = Ways_[way];
  return std::span<const double>(Points_).subspan(source.First * 2, source.Count * 2);
}

}
