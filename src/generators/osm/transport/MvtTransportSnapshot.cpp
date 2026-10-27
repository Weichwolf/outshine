#include "MvtTransportSnapshotInternal.h"
#include "OsmField.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "StreetField.h"
#include "Wayfinding.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <tuple>
#include <utility>
#include <type_traits>
#include <string_view>
#include <vector>

namespace outshine::Generators::Osm {
namespace MvtTransport {
namespace {
bool MatchesSource(const OsmField &field,
                   const StreetField &streets,
                   const Path::Network &network) {
  const auto points = field.Points();
  for (size_t at = 0; at < network.WayCount(); ++at) {
    const size_t tag = network.TagOf(at);
    if (tag >= streets.Ways().size()) { return false; }
    const auto &street = streets.Ways()[tag];
    const size_t first = static_cast<size_t>(street.FirstPoint) * 2;
    const size_t count = static_cast<size_t>(street.PointCount) * 2;
    if (first > points.size() || count > points.size() - first ||
        !std::ranges::equal(network.CoordinatesOfWay(at),
                            std::span(points).subspan(first, count))) {
      return false;
    }
  }
  return true;
}

World::TransportEdge Edge(const Way &way, const Segment &segment, uint32_t ordinal, bool reverse) {
  const auto &attributes = way.Attributes;
  return {
      .Id = {.PathId = Identifier(segment.Key),
             .SegmentOrdinal = ordinal,
             .Direction = reverse ? World::EdgeDirection::Reverse : World::EdgeDirection::Forward},
      .FromNodeId = reverse ? segment.ToId : segment.FromId,
      .ToNodeId = reverse ? segment.FromId : segment.ToId,
      .Modes = attributes.Modes,
      .Facility = attributes.Facility,
      .Surface = attributes.Surface,
      .WidthM = attributes.WidthM,
      .LaneCount = attributes.LaneCount,
      .Layer = attributes.Layer,
      .Bridge = attributes.Bridge,
      .Tunnel = attributes.Tunnel,
      .Access = attributes.Access};
}

std::expected<std::vector<World::TransportEdge>, std::string>
Edges(Sources &sources, std::vector<Segment> &segments) {
  std::ranges::sort(segments, [](const Segment &a, const Segment &b) {
    return std::tuple{Identifier(a.Key), a.Key, a.FromId, a.ToId} <
           std::tuple{Identifier(b.Key), b.Key, b.FromId, b.ToId};
  });
  std::vector<World::TransportEdge> edges;
  edges.reserve(segments.size() * 2);
  uint64_t previous = 0;
  Signature previousKey{};
  uint32_t ordinal = 0;
  for (auto &segment : segments) {
    const auto &way = sources.Ways[segment.Way];
    const uint64_t id = Identifier(segment.Key);
    if (id == 0 || (id == previous && segment.Key != previousKey)) {
      return std::unexpected("MVT transport path identity collision");
    }
    if (id != previous) {
      if (std::cmp_greater(segment.Way, std::numeric_limits<uint32_t>::max())) {
        return std::unexpected("MVT transport source way exceeds the local index range");
      }
      if (sources.Provenance.References.size() >= std::numeric_limits<uint32_t>::max()) {
        return std::unexpected("MVT transport has too many source references");
      }
      sources.Provenance.Paths.push_back(
          {.PathId = id,
           .FirstReference = static_cast<uint32_t>(sources.Provenance.References.size()),
           .ReferenceCount = 1,
           .SuppliedFields = way.Supplied,
           .DerivedFields = way.Derived});
      sources.StreetWays.push_back(static_cast<uint32_t>(segment.Way));
      auto reference = way.Source;
      reference.SegmentOrdinal = segment.SourceStep;
      sources.Provenance.References.push_back(reference);
      ordinal = 0;
    } else {
      if (ordinal == std::numeric_limits<uint32_t>::max()) {
        return std::unexpected("MVT transport has too many segment fragments");
      }
      ++ordinal;
    }
    segment.FragmentOrdinal = ordinal;
    if (way.Attributes.Travel != OsmWayTravel::Reverse) {
      edges.push_back(Edge(way, segment, ordinal, false));
    }
    if (way.Attributes.Travel != OsmWayTravel::Forward) {
      edges.push_back(Edge(way, segment, ordinal, true));
    }
    previous = id;
    previousKey = segment.Key;
  }
  return edges;
}

template <class T> void HashValue(Sha256Prefix &hash, T value) {
  if constexpr (std::is_enum_v<T>) {
    HashValue(hash, static_cast<uint32_t>(value));
  } else if constexpr (std::is_same_v<T, bool>) {
    HashValue(hash, static_cast<uint8_t>(value));
  } else {
    const auto encoded = LittleEndian(value);
    hash.Append(&encoded, sizeof(encoded));
  }
}

template <class... Values> void HashValues(Sha256Prefix &hash, Values... values) {
  (HashValue(hash, values), ...);
}

Data::SourceIdentity Identity(const Sources &sources,
                              std::span<const World::TransportNode> nodes,
                              std::span<const World::TransportEdge> edges) {
  constexpr std::string_view kNamespace = "outshine/mvt/transport/1";
  Sha256Prefix hash(kNamespace.data(), kNamespace.size());
  HashValues(hash,
             static_cast<uint64_t>(sources.Provenance.Sources.size()),
             static_cast<uint64_t>(nodes.size()),
             static_cast<uint64_t>(edges.size()));
  for (const auto &source : sources.Provenance.Sources) {
    const auto signature = SourceSignature(source);
    hash.Append(signature.data(), signature.size());
  }
  for (const auto &node : nodes) {
    HashValues(hash, node.SourceNodeId, node.LatitudeDeg, node.LongitudeDeg);
  }
  for (const auto &edge : edges) {
    HashValues(hash,
               edge.Id.PathId,
               edge.Id.SegmentOrdinal,
               edge.Id.Direction,
               edge.FromNodeId,
               edge.ToNodeId,
               edge.Modes,
               edge.Facility,
               edge.Surface,
               edge.WidthM,
               edge.LaneCount,
               edge.Layer,
               edge.Bridge,
               edge.Tunnel,
               edge.Access);
  }
  return {.DatasetId = "outshine/mvt/transport", .Revision = hash.Hex(nullptr, 0)};
}
}

std::expected<MvtTransportSnapshot, std::string> Publish(Sources sources,
                                                         const Path::Network &network,
                                                         const OsmField &field,
                                                         const StreetField &streets,
                                                         std::span<const GeographicBounds> coverage,
                                                         const std::stop_token &stop,
                                                         MvtTransportCapture capture) {
  auto connections = ReadConnections(sources, network, stop);
  if (!connections) { return std::unexpected(connections.error()); }
  NodeGroupCapture nodeGroups{.BytesMost = capture.NodeGroupPayloadBytesMost, .Data = {}};
  auto nodes = AssignNodes(sources, connections->Segments, network, nodeGroups, stop);
  if (!nodes) { return std::unexpected(nodes.error()); }
  auto edges = Edges(sources, connections->Segments);
  if (!edges) { return std::unexpected(edges.error()); }
  if (auto used = RecordMissingTopology(sources, stop); !used) {
    return std::unexpected(std::move(used.error()));
  }
  std::ranges::sort(*nodes, {}, &World::TransportNode::SourceNodeId);
  std::ranges::sort(*edges, {}, &World::TransportEdge::Id);
  const auto omissionKey = [](const MvtTransportOmission &omission) {
    return std::tie(omission.Source, omission.Feature, omission.Reason);
  };
  std::ranges::sort(sources.Omissions, {}, omissionKey);
  const auto duplicate = std::ranges::unique(sources.Omissions, {}, omissionKey);
  sources.Omissions.erase(duplicate.begin(), duplicate.end());
  const auto identity = Identity(sources, *nodes, *edges);
  if (nodeGroups.Data) {
    CanonicalizeNodeGroups(*nodeGroups.Data, identity.Revision);
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
  }
  auto snapshot = std::make_shared<World::TransportNetworkSnapshot>(
      World::ResolvedTransport{
          .Graph = World::TransportTopology(identity, std::move(*nodes), std::move(*edges)),
          .Routes = {}},
      std::vector<GeographicBounds>(coverage.begin(), coverage.end()),
      World::TransportLoadMetrics{},
      std::move(sources.Provenance));
  if (!World::ValidateTransportNetwork(*snapshot)) {
    return std::unexpected("MVT transport normalization produced an invalid native snapshot");
  }
  SourceFactCapture facts{};
  if (capture.SourceFactsPayloadBytesMost != 0) {
    auto captured = CaptureSourceFacts(sources,
                                       connections->Segments,
                                       network,
                                       field,
                                       streets,
                                       snapshot,
                                       nodeGroups.Data,
                                       capture.SourceFactsPayloadBytesMost,
                                       stop);
    if (!captured) { return std::unexpected(std::move(captured.error())); }
    facts = std::move(*captured);
  }
  return MvtTransportSnapshot{.Network = std::move(snapshot),
                              .Omissions = std::move(sources.Omissions),
                              .InputConnections = connections->InputCount,
                              .StreetWays = std::move(sources.StreetWays),
                              .SourceOrigin = {},
                              .NodeGroupState = nodeGroups.State,
                              .NodeGroups = std::move(nodeGroups.Data),
                              .SourceFactState = facts.State,
                              .SourceFacts = std::move(facts.Data)};
}
}

std::expected<MvtTransportSnapshot, std::string>
BuildMvtTransportSnapshot(const OsmField &field,
                          const StreetField &streets,
                          const Path::Network &network,
                          std::span<const GeographicBounds> coverage,
                          const std::stop_token &stop,
                          MvtTransportCapture capture) {
  if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
  if (!MvtTransport::MatchesSource(field, streets, network)) {
    return std::unexpected("MVT transport graph coordinates do not match the actual source lease");
  }
  auto sources = MvtTransport::PrepareSources(field, streets, stop);
  if (!sources) { return std::unexpected(sources.error()); }
  auto result =
      MvtTransport::Publish(std::move(*sources), network, field, streets, coverage, stop, capture);
  if (result) {
    result->SourceOrigin = field.ShareOriginToken();
    result->SourceGeneration = field.Generation();
    result->SourceWayCount = streets.Ways().size();
  }
  return result;
}
}
