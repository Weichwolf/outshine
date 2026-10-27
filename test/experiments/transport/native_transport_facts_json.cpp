#include "native_transport_json.h"
#include "MvtTransportSourceFacts.h"
#include "OsmField.h"
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <print>
#include <span>

namespace outshine::Test::TransportCapture {
using namespace Generators::Osm;

void WriteCoordinate(std::FILE *file, double value) {
  if (value == 0.0) {
    std::print(file, "{}.0", std::signbit(value) ? "-0" : "0");
  } else {
    std::print(file, "{}", value);
  }
}

void WriteBytes(std::FILE *f, std::span<const uint8_t> bytes) {
  std::print(f, "[");
  for (size_t i = 0; i < bytes.size(); ++i) {
    std::print(f, "{}{}", i != 0 ? "," : "", static_cast<unsigned>(bytes[i]));
  }
  std::print(f, "]");
}

void WriteSourceFacts(std::FILE *f,
                      const MvtTransportSourceFacts &facts,
                      const OsmField &field,
                      const std::array<int, 3> &tile) {
  const auto &identity = facts.Network->SourceIdentity();
  std::print(
      f,
      R"({{"schema":"outshine/native-mvt-source-facts/2","InputSha256":"{}","Tile":[{},{},{}],"NetworkIdentity":{{"DatasetId":"{}","Revision":"{}"}},"SourceGeneration":{},"SourceWayCount":{},"Digest":)",
      field.Tiles().front().InputDigest,
      tile[0],
      tile[1],
      tile[2],
      identity.DatasetId,
      identity.Revision,
      facts.SourceGeneration,
      facts.SourceWayCount);
  WriteBytes(f, facts.Digest);
  std::print(f, R"(,"NodeGroups":{{"Digest":)");
  WriteBytes(f, facts.NodeGroups->Digest);
  std::print(f, R"(,"Nodes":[)");
  for (size_t i = 0; i < facts.NodeGroups->Nodes.size(); ++i) {
    std::print(f, "{}{}", i != 0 ? "," : "", facts.NodeGroups->Nodes[i]);
  }
  std::print(f, R"(],"Groups":[)");
  size_t at = 0;
  for (const auto &group : facts.NodeGroups->Groups) {
    std::print(f,
               R"({}{{"FirstNode":{},"NodeCount":{},"SourceConnectionCount":{}}})",
               at++ != 0 ? "," : "",
               group.FirstNode,
               group.NodeCount,
               group.SourceConnectionCount);
  }
  std::print(f, "]}}");
  std::print(f, R"(,"Groups":[)");
  at = 0;
  for (const auto &g : facts.Groups) {
    std::print(
        f,
        R"({}{{"FirstMember":{},"MemberCount":{},"OriginalEndpointCount":{},"FilteredEndpointCount":{},"Complete":{},"Frontier":{}}})",
        at++ != 0 ? "," : "",
        g.FirstMember,
        g.MemberCount,
        g.OriginalEndpointCount,
        g.FilteredEndpointCount,
        g.Complete,
        g.Frontier);
  }
  std::print(f, R"(],"Members":[)");
  at = 0;
  for (const auto &m : facts.Members) {
    std::print(
        f,
        R"({}{{"Edge":{{"PathId":{},"SegmentOrdinal":{},"Direction":{}}},"DomainNode":{},"NodeGroup":{},"StreetWay":{},"SourceStep":{},"End":{},"SourceTerminal":{}}})",
        at++ != 0 ? "," : "",
        m.Edge.PathId,
        m.Edge.SegmentOrdinal,
        static_cast<unsigned>(m.Edge.Direction),
        m.DomainNode,
        m.NodeGroup,
        m.StreetWay,
        m.SourceStep,
        static_cast<unsigned>(m.End),
        m.SourceTerminal);
  }
  std::print(f, R"(],"Ways":[)");
  at = 0;
  for (const auto &w : facts.Ways) {
    std::print(
        f,
        R"({}{{"StreetWay":{},"Feature":{},"Ring":{},"PointCount":{},"HalfWidthWord":{},"MaximumGradientWord":{},"OwnsEnds":[{},{}],"EndStencil":[)",
        at++ != 0 ? "," : "",
        w.StreetWay,
        w.Feature,
        w.Ring,
        w.PointCount,
        w.HalfWidthWord,
        w.MaximumGradientWord,
        w.OwnsEnds[0],
        w.OwnsEnds[1]);
    for (size_t i = 0; i < w.EndStencil.size(); ++i) {
      std::print(f, R"({}{{"LongitudeDeg":)", i != 0 ? "," : "");
      WriteCoordinate(f, w.EndStencil[i].LongitudeDeg);
      std::print(f, R"(,"LatitudeDeg":)");
      WriteCoordinate(f, w.EndStencil[i].LatitudeDeg);
      std::print(f, "}}");
    }
    std::print(f, "]}}");
  }
  std::print(f, "]}}\n");
}
}
