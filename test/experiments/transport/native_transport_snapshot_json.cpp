#include "native_transport_json.h"
#include "MvtTransportSnapshot.h"
#include "OsmField.h"
#include "StreetField.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <print>
#include <string_view>

namespace outshine::Test::TransportCapture {
namespace {
using namespace Generators::Osm;

void String(std::FILE *file, std::string_view value) {
  std::fputc('"', file);
  for (const char byte : value) {
    const auto character = static_cast<unsigned char>(byte);
    if (character == '"' || character == '\\') {
      std::fputc('\\', file);
      std::fputc(character, file);
    } else if (character < 0x20) {
      std::print(file, "\\u{:04x}", character);
    } else {
      std::fputc(character, file);
    }
  }
  std::fputc('"', file);
}

void Topology(std::FILE *file, const World::TransportTopology &graph) {
  std::print(file, R"(,"Nodes":[)");
  size_t at = 0;
  for (const auto &node : graph.Nodes()) {
    std::print(file,
               R"({}{{"SourceNodeId":{},"LatitudeDeg":{},"LongitudeDeg":{}}})",
               at++ != 0 ? "," : "",
               node.SourceNodeId,
               node.LatitudeDeg,
               node.LongitudeDeg);
  }
  std::print(file, R"(],"Edges":[)");
  at = 0;
  for (const auto &edge : graph.Edges()) {
    std::print(
        file,
        R"({}{{"Id":{{"PathId":{},"SegmentOrdinal":{},"Direction":{}}},"FromNodeId":{},"ToNodeId":{},"Modes":{},"Facility":{},"Surface":{},"WidthM":{},"LaneCount":{},"Layer":{},"Bridge":{},"Tunnel":{},"Access":{}}})",
        at++ != 0 ? "," : "",
        edge.Id.PathId,
        edge.Id.SegmentOrdinal,
        static_cast<unsigned>(edge.Id.Direction),
        edge.FromNodeId,
        edge.ToNodeId,
        static_cast<unsigned>(edge.Modes),
        static_cast<unsigned>(edge.Facility),
        static_cast<unsigned>(edge.Surface),
        edge.WidthM,
        static_cast<unsigned>(edge.LaneCount),
        edge.Layer,
        edge.Bridge,
        edge.Tunnel,
        static_cast<unsigned>(edge.Access));
  }
  std::print(file, R"(],"Outgoing":[)");
  at = 0;
  for (const auto &edge : graph.Outgoing()) {
    std::print(file,
               R"({}{{"NodeId":{},"EdgeIndex":{}}})",
               at++ != 0 ? "," : "",
               edge.NodeId,
               edge.EdgeIndex);
  }
  std::print(file, "]");
}

void Sources(std::FILE *file, const World::TransportProvenance &provenance) {
  std::print(file, R"(,"Sources":[)");
  size_t at = 0;
  for (const auto &source : provenance.Sources) {
    std::print(file, R"({}{{"Identity":{{"DatasetId":)", at++ != 0 ? "," : "");
    String(file, source.Identity.DatasetId);
    std::print(file, R"(,"Revision":)");
    String(file, source.Identity.Revision);
    std::print(file, R"(}},"Item":)");
    String(file, source.Item);
    std::print(file, R"(,"Content":)");
    WriteBytes(file, source.Content);
    std::print(file, "}}");
  }
  std::print(file, R"(],"References":[)");
  at = 0;
  for (const auto &reference : provenance.References) {
    std::print(file,
               R"({}{{"Source":{},"SegmentOrdinal":{},"ProviderElementId":)",
               at++ != 0 ? "," : "",
               reference.Source,
               reference.SegmentOrdinal);
    if (reference.ProviderElementId) {
      std::print(file, "{}", *reference.ProviderElementId);
    } else {
      std::print(file, "null");
    }
    std::print(file, R"(,"Fragment":)");
    WriteBytes(file, reference.Fragment);
    std::print(file, "}}");
  }
  std::print(file, "]");
}

void HeightProfileTags(std::FILE *file, const OsmField &field, const OsmField::Feature &feature) {
  std::print(file, R"(,"HeightProfileTags":{{)");
  bool first = true;
  for (const auto key : {std::string_view("indoor"), std::string_view("level")}) {
    if (!first) { std::print(file, ","); }
    first = false;
    String(file, key);
    std::print(file, ":");
    const auto tag = field.ReadAttribute(feature, key);
    if (!tag) {
      std::print(file, "null");
      continue;
    }
    std::print(file,
               R"({{"IsNumber":{},"NumberWord":{},"String":)",
               tag->IsNumber,
               std::bit_cast<uint64_t>(tag->Number));
    String(file, tag->String);
    std::print(file, "}}");
  }
  std::print(file, "}}");
}

bool Paths(std::FILE *file,
           const MvtTransportSnapshot &snapshot,
           const OsmField &field,
           const StreetField &streets) {
  std::print(file, R"(,"Paths":[)");
  const auto &paths = snapshot.Network->Provenance().Paths;
  for (size_t at = 0; at < paths.size(); ++at) {
    const auto &path = paths[at];
    const auto way = snapshot.StreetWays[at];
    const auto source = streets.SourceOf(field, way);
    if (!source) { return false; }
    const auto &feature = field.Features()[source->Feature];
    std::print(
        file,
        R"({}{{"PathId":{},"FirstReference":{},"ReferenceCount":{},"SuppliedFields":{},"DerivedFields":{},"LocalWayBinding":{{"StreetWay":{},"Feature":{},"Ring":{},"FeatureRingOrdinal":{},"LayerName":)",
        at != 0 ? "," : "",
        path.PathId,
        path.FirstReference,
        path.ReferenceCount,
        path.SuppliedFields,
        path.DerivedFields,
        way,
        source->Feature,
        source->Ring,
        source->Ring - feature.FirstRing);
    String(file, field.LayerName(feature.Layer));
    std::print(file, R"(,"FeatureContent":)");
    WriteBytes(file, feature.ContentDigest);
    HeightProfileTags(file, field, feature);
    std::print(file, "}}}}");
  }
  std::print(file, "]");
  return true;
}
}

bool WriteSnapshot(std::FILE *file,
                   const Generators::Osm::MvtTransportSnapshot &snapshot,
                   const Generators::Osm::OsmField &field,
                   const Generators::Osm::StreetField &streets,
                   const std::array<int, 3> &tile) {
  const auto &network = *snapshot.Network;
  std::print(
      file,
      R"({{"schema":"outshine/native-mvt-snapshot-dump/1","mvt_schema":"openmaptiles","input_sha256":"{}","tile":[{},{},{}],"SourceIdentity":{{"DatasetId":)",
      field.Tiles().front().InputDigest,
      tile[0],
      tile[1],
      tile[2]);
  String(file, network.SourceIdentity().DatasetId);
  std::print(file, R"(,"Revision":)");
  String(file, network.SourceIdentity().Revision);
  std::print(
      file,
      R"(}},"IdentitySpace":{},"InputConnections":{},"SourceWayCount":{},"SourceGeneration":{},"UnclassifiedPathCount":{},"native_capacity_bytes":{},"Coverage":[)",
      static_cast<unsigned>(network.Provenance().IdentitySpace),
      snapshot.InputConnections,
      snapshot.SourceWayCount,
      snapshot.SourceGeneration,
      network.Topology().UnclassifiedPathCount(),
      network.CapacityBytes());
  size_t at = 0;
  for (const auto &box : network.Coverage()) {
    std::print(file,
               R"({}{{"WestDeg":{},"SouthDeg":{},"EastDeg":{},"NorthDeg":{}}})",
               at++ != 0 ? "," : "",
               box.WestDeg,
               box.SouthDeg,
               box.EastDeg,
               box.NorthDeg);
  }
  std::print(file, "]");
  Topology(file, network.Topology());
  Sources(file, network.Provenance());
  if (!Paths(file, snapshot, field, streets)) { return false; }
  std::print(file, R"(,"Omissions":[)");
  at = 0;
  for (const auto &omission : snapshot.Omissions) {
    std::print(file,
               R"({}{{"Source":{},"Reason":{},"Feature":)",
               at++ != 0 ? "," : "",
               omission.Source,
               static_cast<unsigned>(omission.Reason));
    WriteBytes(file, omission.Feature);
    std::print(file, "}}");
  }
  std::print(
      file,
      R"(],"scope":["Actual native input DTOs, no solver oracle","Single tile, no geographic closure claim"]}})");
  return true;
}
}
