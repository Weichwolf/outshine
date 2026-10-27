#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTTRANSPORTSNAPSHOTINTERNAL_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTTRANSPORTSNAPSHOTINTERNAL_H
#include "MvtTransportSnapshot.h"
#include "OsmWaySemantics.h"
#include "TransportProvenance.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators::Osm::MvtTransport {
using Signature = std::array<uint8_t, 32>;

struct Way {
  bool Usable = false;
  uint32_t PointCount = 0;
  OsmWaySemantics Attributes;
  World::TransportSourceReference Source;
  Signature Key{};
  Signature Feature{};
  uint16_t Supplied = 0;
  uint16_t Derived = 0;
};

struct Sources {
  World::TransportProvenance Provenance;
  std::vector<uint32_t> TileRows;
  std::vector<Way> Ways;
  std::vector<uint32_t> StreetWays;
  std::vector<MvtTransportOmission> Omissions;
};

struct Segment {
  Signature Key{};
  uint32_t Way = 0;
  uint32_t NativeWay = 0;
  size_t From = 0;
  size_t To = 0;
  uint32_t SourceStep = 0;
  uint32_t FragmentOrdinal = 0;
  uint64_t FromId = 0;
  uint64_t ToId = 0;
};

struct Connections {
  std::vector<Segment> Segments;
  size_t InputCount = 0;
};

struct NodeGroupCapture {
  size_t BytesMost = 0;
  MvtTransportNodeGroupState State = MvtTransportNodeGroupState::NotRequested;
  std::shared_ptr<MvtTransportNodeGroups> Data;
};

void CanonicalizeNodeGroups(MvtTransportNodeGroups &groups, std::string_view revision);

struct SourceFactCapture {
  MvtTransportSourceFactState State = MvtTransportSourceFactState::NotRequested;
  std::shared_ptr<const MvtTransportSourceFacts> Data;
};

[[nodiscard]] std::expected<SourceFactCapture, std::string>
CaptureSourceFacts(const Sources &sources,
                   std::span<const Segment> segments,
                   const Path::Network &network,
                   const OsmField &field,
                   const StreetField &streets,
                   std::shared_ptr<const World::TransportNetworkSnapshot> snapshot,
                   std::shared_ptr<const MvtTransportNodeGroups> groups,
                   size_t bytesMost,
                   const std::stop_token &stop);
void BindSourceFacts(MvtTransportSourceFacts &facts);
[[nodiscard]] std::array<uint8_t, 32> SourceFactDigest(const MvtTransportSourceFacts &facts);
[[nodiscard]] std::array<uint8_t, 32>
GroundInputOmissionDigest(std::span<const MvtTransportOmission> omissions);

[[nodiscard]] std::expected<Connections, std::string>
ReadConnections(const Sources &sources, const Path::Network &network, const std::stop_token &stop);
[[nodiscard]] std::expected<void, std::string> RecordMissingTopology(Sources &sources,
                                                                     const std::stop_token &stop);
[[nodiscard]] std::expected<std::vector<World::TransportNode>, std::string>
AssignNodes(Sources &sources,
            std::vector<Segment> &segments,
            const Path::Network &network,
            NodeGroupCapture &capture,
            const std::stop_token &stop);

[[nodiscard]] std::expected<Sources, std::string> SourceRows(const OsmField &field);
[[nodiscard]] Signature SourceSignature(const World::TransportSource &source);
[[nodiscard]] Signature WaySignature(const Signature &source,
                                     const Signature &feature,
                                     std::string_view layer,
                                     uint32_t ring);
[[nodiscard]] std::expected<Way, std::string>
DescribeWay(const OsmField &field, const StreetField &streets, size_t index);
[[nodiscard]] std::expected<Sources, std::string>
PrepareSources(const OsmField &field, const StreetField &streets, const std::stop_token &stop);
[[nodiscard]] Signature SegmentSignature(const Signature &way, uint32_t step);
[[nodiscard]] uint64_t Identifier(const Signature &signature) noexcept;
[[nodiscard]] std::expected<MvtTransportSnapshot, std::string>
Publish(Sources sources,
        const Path::Network &network,
        const OsmField &field,
        const StreetField &streets,
        std::span<const GeographicBounds> coverage,
        const std::stop_token &stop,
        MvtTransportCapture capture);
}
#endif
