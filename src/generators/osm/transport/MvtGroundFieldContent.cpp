#include "MvtGroundFieldScopeInternal.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace outshine::Generators::Osm::GroundFieldScopes {
namespace {
template <class T> void Append(Sha256Prefix &hash, T value) {
  const auto word = LittleEndian(value);
  hash.Append(&word, sizeof(word));
}

void Text(Sha256Prefix &hash, std::string_view text) {
  Append(hash, static_cast<uint64_t>(text.size()));
  hash.Append(text.data(), text.size());
}

void Edge(Sha256Prefix &hash, const World::TransportEdgeId &edge) {
  Append(hash, edge.PathId);
  Append(hash, edge.SegmentOrdinal);
  Append(hash, static_cast<uint8_t>(edge.Direction));
}

void Member(Sha256Prefix &hash, const State &state, const MvtTransportSourceMember &member) {
  Edge(hash, member.Edge);
  Append(hash, member.DomainNode);
  Append(hash, member.SourceStep);
  Append(hash, member.End);
  Append(hash, static_cast<uint8_t>(member.SourceTerminal));
  const auto &way = state.Facts.Ways[state.Ways[member.StreetWay]];
  Append(hash, way.PointCount);
  Append(hash, way.HalfWidthWord);
  Append(hash, way.MaximumGradientWord);
  for (const auto point : way.EndStencil) {
    Append(hash, point.LongitudeDeg);
    Append(hash, point.LatitudeDeg);
  }
  for (const auto owns : way.OwnsEnds) { Append(hash, static_cast<uint8_t>(owns)); }
}

Checked<void> Groups(Sha256Prefix &hash, State &state) {
  const auto nodes = [&](uint32_t group) {
    return state.Facts.NodeGroups->NodesOf(state.Facts.NodeGroups->Groups[group]);
  };
  std::ranges::sort(state.Groups, [&](uint32_t a, uint32_t b) {
    return std::ranges::lexicographical_compare(nodes(a), nodes(b));
  });
  Append(hash, static_cast<uint64_t>(state.Groups.size()));
  for (const auto groupIndex : state.Groups) {
    const auto ids = nodes(groupIndex);
    Append(hash, static_cast<uint64_t>(ids.size()));
    for (const auto node : ids) { Append(hash, node); }
    Append(hash, state.Facts.NodeGroups->Groups[groupIndex].SourceConnectionCount);
    const auto &group = state.Facts.Groups[groupIndex];
    Append(hash, group.MemberCount);
    Append(hash, group.OriginalEndpointCount);
    Append(hash, group.FilteredEndpointCount);
    Append(hash, static_cast<uint8_t>(group.Complete));
    Append(hash, static_cast<uint8_t>(group.Frontier));
    for (const auto &member :
         std::span(state.Facts.Members).subspan(group.FirstMember, group.MemberCount)) {
      if (state.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
      if (state.Output.Work.OriginalMemberVisits + state.Output.Work.QualifiedMemberVisits +
              state.Output.Work.ContentMemberVisits >=
          state.Limits.MemberVisits) {
        return std::unexpected(Error::CapacityExceeded);
      }
      ++state.Output.Work.ContentMemberVisits;
      Member(hash, state, member);
    }
  }
  return {};
}
}

Checked<void> BindContent(State &state) {
  constexpr std::string_view kVersion = "outshine/finite-ground-content/1";
  Sha256Prefix hash(kVersion.data(), kVersion.size());
  const auto &source = state.Source();
  Text(hash, source.Source->SourceIdentity().DatasetId);
  Text(hash, source.Source->SourceIdentity().Revision);
  const auto &profiles = state.Inputs.Ports().ProfileInput.Content;
  hash.Append(profiles.data(), profiles.size());
  const auto &omissions = state.Inputs.OmissionBinding();
  hash.Append(omissions.data(), omissions.size());
  const auto &graph = source.Source->Topology();
  Append(hash, static_cast<uint64_t>(source.Nodes.size()));
  for (const auto node : source.Nodes) { Append(hash, graph.Nodes()[node].SourceNodeId); }
  Append(hash, static_cast<uint64_t>(source.SourceEdges.size()));
  for (const auto edge : source.SourceEdges) { Edge(hash, graph.Edges()[edge].Id); }
  Append(hash, source.WidthM);
  Append(hash, source.MaximumGradient);
  if (auto groups = Groups(hash, state); !groups) { return groups; }
  state.Output.Field.Info.ContentBinding = hash.Digest(nullptr, 0);
  return {};
}
}
