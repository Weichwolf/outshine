#include "MvtTransportSnapshotInternal.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
template <class T> void Append(Sha256Prefix &hash, T value) {
  const auto word = LittleEndian(value);
  hash.Append(&word, sizeof(word));
}
}

void CanonicalizeNodeGroups(MvtTransportNodeGroups &groups, std::string_view revision) {
  for (const auto &group : groups.Groups) {
    std::ranges::sort(std::span(groups.Nodes).subspan(group.FirstNode, group.NodeCount));
  }
  std::ranges::sort(groups.Groups, {}, [&groups](const MvtTransportNodeGroup &group) {
    return groups.Nodes[group.FirstNode];
  });
  std::vector<uint64_t> nodes;
  nodes.reserve(groups.Nodes.size());
  constexpr std::string_view kVersion = "outshine/mvt-node-groups/1";
  Sha256Prefix hash(kVersion.data(), kVersion.size());
  hash.Append(revision.data(), revision.size());
  Append(hash, static_cast<uint64_t>(groups.Groups.size()));
  Append(hash, static_cast<uint64_t>(groups.Nodes.size()));
  for (auto &group : groups.Groups) {
    const auto original = groups.NodesOf(group);
    group.FirstNode = static_cast<uint32_t>(nodes.size());
    Append(hash, group.NodeCount);
    Append(hash, group.SourceConnectionCount);
    for (const uint64_t node : original) {
      nodes.push_back(node);
      Append(hash, node);
    }
  }
  groups.Nodes = std::move(nodes);
  groups.Digest = hash.Digest(nullptr, 0);
}
}
