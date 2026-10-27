#include "MvtGroundFieldScopeInternal.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <tuple>

namespace outshine::Generators::Osm::GroundFieldScopes {
namespace {
template <class T> void Append(Sha256Prefix &hash, T value) {
  const auto word = LittleEndian(value);
  hash.Append(&word, sizeof(word));
}

void Edge(Sha256Prefix &hash, const World::TransportEdgeId &edge) {
  Append(hash, edge.PathId);
  Append(hash, edge.SegmentOrdinal);
  Append(hash, static_cast<uint8_t>(edge.Direction));
}
}

void Bind(State &state) {
  auto &scope = state.Output.Field;
  auto &input = scope.Info.Input;
  input.DatasetId = "outshine/finite-ground-domain";
  input.Version = "1";
  input.Revision = scope.Source.Source->SourceIdentity().Revision;
  input.ItemId = "original-member-scope";
  constexpr std::string_view kVersion = "outshine/finite-ground-domain/1";
  Sha256Prefix hash(kVersion.data(), kVersion.size());
  hash.Append(state.Facts.Digest.data(), state.Facts.Digest.size());
  hash.Append(state.Inputs.Ports().ProfileInput.Content.data(),
              state.Inputs.Ports().ProfileInput.Content.size());
  hash.Append(state.Inputs.OmissionBinding().data(), state.Inputs.OmissionBinding().size());
  Append(hash, static_cast<uint64_t>(scope.Source.Nodes.size()));
  for (const auto node : scope.Source.Nodes) {
    Append(hash, scope.Source.Source->Topology().Nodes()[node].SourceNodeId);
  }
  Append(hash, static_cast<uint64_t>(scope.Source.SourceEdges.size()));
  for (const auto edge : scope.Source.SourceEdges) {
    Edge(hash, scope.Source.Source->Topology().Edges()[edge].Id);
  }
  Append(hash, scope.Source.WidthM);
  Append(hash, scope.Source.MaximumGradient);
  Append(hash, static_cast<uint64_t>(state.Groups.size()));
  for (const auto group : state.Groups) { Append(hash, group); }
  std::ranges::sort(scope.Info.Exits, {}, [](const GroundPortFieldExit &exit) {
    return std::tuple{
        exit.SourceGroup, exit.Ground, exit.GroundEnd, exit.External, exit.ExternalEnd};
  });
  Append(hash, static_cast<uint64_t>(scope.Info.Exits.size()));
  for (const auto &exit : scope.Info.Exits) {
    Append(hash, exit.SourceGroup);
    Edge(hash, exit.Ground);
    Edge(hash, exit.External);
    Append(hash, exit.ExternalNode);
    Append(hash, exit.GroundEnd);
    Append(hash, exit.ExternalEnd);
    Append(hash, exit.Layer);
    Append(hash, static_cast<uint8_t>(exit.Bridge));
    Append(hash, static_cast<uint8_t>(exit.Tunnel));
  }
  input.Content = hash.Digest(nullptr, 0);
  state.Output.Work.ContainerCapacityBytes += input.DatasetId.capacity() +
                                              input.Version.capacity() + input.Revision.capacity() +
                                              input.ItemId.capacity() + 4;
}
}
