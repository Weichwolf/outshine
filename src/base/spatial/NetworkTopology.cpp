#include "Wayfinding.h"
#include "Earth.h"
#include <algorithm>
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

bool Network::UsesTopologyGrid(Snap snap, Sphere sphere) const noexcept {
  return Woven_ && SnapM_ == snap.CellM && RadiusM_ == sphere.RadiusM;
}

bool Network::MatchesWay(size_t way,
                         std::span<const double> coordinates,
                         const WayClass &properties) const noexcept {
  if (!Woven_ || way >= Ways_.size()) { return false; }
  const auto &held = Ways_[way];
  const WayClass layout{.HalfWidthM = held.HalfWidthM,
                        .MaxGradient = held.MaxGradient,
                        .MinRadiusM = held.MinRadiusM,
                        .Friction = held.Friction,
                        .SpeedMps = held.SpeedMps,
                        .Lanes = held.Lanes,
                        .Priority = held.Priority,
                        .Oneway = held.Oneway,
                        .Sealed = held.Sealed,
                        .Spans = held.Spans,
                        .Tag = held.Tag};
  return layout == properties && std::ranges::equal(CoordinatesOfWay(way), coordinates);
}

std::optional<size_t> Network::NodeOfWayPoint(size_t way, size_t point) const noexcept {
  if (!Woven_ || way >= Ways_.size() || point >= Ways_[way].Count) { return {}; }
  return NodeOfPoint_[Ways_[way].First + point];
}

}
