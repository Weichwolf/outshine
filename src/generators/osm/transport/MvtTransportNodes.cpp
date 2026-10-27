#include "MvtTransportSnapshotInternal.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include "Wayfinding.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <tuple>
#include <vector>
#include <utility>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
struct Endpoint {
  size_t Node = 0;
  size_t Segment = 0;
  int32_t Layer = 0;
  bool Bridge = false;
  bool Tunnel = false;
  bool End = false;
  Signature Path{};
};

auto Domain(const Endpoint &endpoint) {
  return std::tie(endpoint.Node, endpoint.Layer, endpoint.Bridge, endpoint.Tunnel);
}

std::vector<Endpoint> Endpoints(const Sources &sources, const std::vector<Segment> &segments) {
  std::vector<Endpoint> out;
  out.reserve(segments.size() * 2);
  for (size_t at = 0; at < segments.size(); ++at) {
    const auto &segment = segments[at];
    const auto &attributes = sources.Ways[segment.Way].Attributes;
    for (const bool end : {false, true}) {
      out.push_back({.Node = end ? segment.To : segment.From,
                     .Segment = at,
                     .Layer = attributes.Layer,
                     .Bridge = attributes.Bridge,
                     .Tunnel = attributes.Tunnel,
                     .End = end,
                     .Path = segment.Key});
    }
  }
  std::ranges::sort(out, [](const Endpoint &a, const Endpoint &b) {
    return std::tuple{Domain(a), a.Path, a.End} < std::tuple{Domain(b), b.Path, b.End};
  });
  return out;
}

Signature NodeKey(std::span<const Endpoint> endpoints, LongitudeLatitude position) {
  constexpr size_t kDomainBytes = sizeof(double) * 2 + sizeof(int32_t) + sizeof(uint8_t) * 2;
  std::array<uint8_t, kDomainBytes> fields{};
  auto *at = fields.data();
  const auto append = [&at](auto value) {
    const auto encoded = LittleEndian(value);
    std::memcpy(at, &encoded, sizeof(encoded));
    at += sizeof(encoded);
  };
  const auto &domain = endpoints.front();
  append(position.LatitudeDeg == 0.0 ? 0.0 : position.LatitudeDeg);
  append(position.LongitudeDeg == 0.0 ? 0.0 : position.LongitudeDeg);
  append(domain.Layer);
  append(static_cast<uint8_t>(domain.Bridge));
  append(static_cast<uint8_t>(domain.Tunnel));
  Sha256Prefix prefix(fields.data(), fields.size());
  Signature previous{};
  bool previousEnd = false;
  bool first = true;
  for (const auto &endpoint : endpoints) {
    if (!first && endpoint.Path == previous && endpoint.End == previousEnd) { continue; }
    prefix.Append(endpoint.Path.data(), endpoint.Path.size());
    const auto end = static_cast<uint8_t>(endpoint.End);
    prefix.Append(&end, sizeof(end));
    previous = endpoint.Path;
    previousEnd = endpoint.End;
    first = false;
  }
  return prefix.Digest(nullptr, 0);
}

void BindEndpoints(Sources &sources,
                   std::vector<Segment> &segments,
                   std::span<const Endpoint> group,
                   uint64_t id,
                   bool ambiguous) {
  for (const auto &endpoint : group) {
    auto &segment = segments[endpoint.Segment];
    (endpoint.End ? segment.ToId : segment.FromId) = id;
    if (!ambiguous) { continue; }
    const auto &way = sources.Ways[segment.Way];
    sources.Omissions.push_back({.Source = way.Source.Source,
                                 .Feature = way.Feature,
                                 .Reason = MvtTransportOmissionReason::UnqualifiedLevelConnection});
  }
}

std::expected<void, std::string> PrepareNodeGroups(NodeGroupCapture &capture,
                                                   std::span<const Endpoint> endpoints,
                                                   const std::stop_token &stop) {
  if (capture.BytesMost == 0) { return {}; }
  capture.State = MvtTransportNodeGroupState::BudgetExceeded;
  size_t groups = 0;
  size_t nodes = 0;
  for (size_t at = 0; at < endpoints.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    groups += static_cast<size_t>(at == 0 || endpoints[at - 1].Node != endpoints[at].Node);
    nodes += static_cast<size_t>(at == 0 || Domain(endpoints[at - 1]) != Domain(endpoints[at]));
  }
  if (groups > std::numeric_limits<uint32_t>::max() ||
      nodes > std::numeric_limits<uint32_t>::max() ||
      capture.BytesMost < sizeof(MvtTransportNodeGroups)) {
    return {};
  }
  size_t left = capture.BytesMost - sizeof(MvtTransportNodeGroups);
  if (groups > left / sizeof(MvtTransportNodeGroup)) { return {}; }
  left -= groups * sizeof(MvtTransportNodeGroup);
  if (nodes > left / (sizeof(uint64_t) * 2)) { return {}; }
  capture.Data = std::make_shared<MvtTransportNodeGroups>();
  capture.Data->Groups.reserve(groups);
  capture.Data->Nodes.reserve(nodes);
  capture.State = MvtTransportNodeGroupState::Captured;
  return {};
}

void RecordNodeGroup(const NodeGroupCapture &capture,
                     std::span<const Endpoint> endpoints,
                     size_t first,
                     uint64_t id,
                     const Path::Network &network) {
  if (!capture.Data) { return; }
  auto &data = *capture.Data;
  if (first == 0 || endpoints[first - 1].Node != endpoints[first].Node) {
    data.Groups.push_back({.FirstNode = static_cast<uint32_t>(data.Nodes.size()),
                           .NodeCount = 0,
                           .SourceConnectionCount = static_cast<uint64_t>(
                               network.ConnectionsFrom(endpoints[first].Node).size())});
  }
  data.Nodes.push_back(id);
  ++data.Groups.back().NodeCount;
}

}

std::expected<std::vector<World::TransportNode>, std::string>
AssignNodes(Sources &sources,
            std::vector<Segment> &segments,
            const Path::Network &network,
            NodeGroupCapture &capture,
            const std::stop_token &stop) {
  const auto endpoints = Endpoints(sources, segments);
  if (auto prepared = PrepareNodeGroups(capture, endpoints, stop); !prepared) {
    return std::unexpected(std::move(prepared.error()));
  }
  std::vector<World::TransportNode> nodes;
  std::vector<std::pair<uint64_t, Signature>> keys;
  for (size_t at = 0; at < endpoints.size();) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    size_t to = at + 1;
    while (to < endpoints.size() && Domain(endpoints[at]) == Domain(endpoints[to])) { ++to; }
    const auto position = network.PositionOfNode(endpoints[at].Node);
    if (!position) { return std::unexpected("MVT transport graph has no published node position"); }
    const auto group = std::span(endpoints).subspan(at, to - at);
    const Signature key = NodeKey(group, *position);
    const uint64_t id = Identifier(key);
    if (id == 0) { return std::unexpected("MVT transport node identity is reserved"); }
    keys.emplace_back(id, key);
    nodes.push_back({.SourceNodeId = id,
                     .LatitudeDeg = position->LatitudeDeg,
                     .LongitudeDeg = position->LongitudeDeg});
    const bool ambiguous = (at > 0 && endpoints[at - 1].Node == endpoints[at].Node) ||
                           (to < endpoints.size() && endpoints[to].Node == endpoints[at].Node);
    BindEndpoints(sources, segments, group, id, ambiguous);
    RecordNodeGroup(capture, endpoints, at, id, network);
    at = to;
  }
  std::ranges::sort(keys);
  for (size_t at = 1; at < keys.size(); ++at) {
    if (keys[at - 1].first == keys[at].first) {
      return std::unexpected(keys[at - 1].second == keys[at].second
                                 ? "MVT transport has ambiguous coincident source nodes"
                                 : "MVT transport node identity collision");
    }
  }
  return nodes;
}
}
