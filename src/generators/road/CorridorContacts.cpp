#include "Corridors.h"

#include "StreetField.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <ratio>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Connection = std::pair<uint32_t, uint32_t>;
using ContactNode = std::pair<uint64_t, int>;

struct NodeHash {
  size_t operator()(const ContactNode &node) const noexcept {
    return std::hash<uint64_t>{}(node.first) ^ std::hash<int>{}(node.second);
  }
};

struct Neighbors {
  std::vector<size_t> First;
  std::vector<uint32_t> Lanes;
};

Neighbors AlongConnections(size_t lanes, std::span<const Connection> connections) {
  Neighbors neighbors{.First = std::vector<size_t>(lanes + 1), .Lanes = {}};
  for (const auto &[first, second] : connections) {
    ++neighbors.First[first + 1];
    ++neighbors.First[second + 1];
  }
  std::partial_sum(neighbors.First.begin(), neighbors.First.end(), neighbors.First.begin());
  neighbors.Lanes.resize(neighbors.First.back());
  auto next = neighbors.First;
  for (const auto &[first, second] : connections) {
    neighbors.Lanes[next[first]++] = second;
    neighbors.Lanes[next[second]++] = first;
  }
  return neighbors;
}

size_t AssignContactKeys(std::span<const Osm::StreetField::Way> ways,
                         const Neighbors &neighbors,
                         std::vector<uint64_t> &keys) {
  keys.assign(ways.size(), 0);
  std::vector<uint32_t> queue;
  queue.reserve(ways.size());
  size_t groups = 0;
  for (size_t first = 0; first < ways.size(); ++first) {
    if (keys[first] != 0) { continue; }
    ++groups;
    queue.clear();
    queue.push_back(static_cast<uint32_t>(first));
    keys[first] = std::numeric_limits<uint64_t>::max();
    uint64_t key = static_cast<uint64_t>(ways[first].FirstPoint) + 1;
    for (size_t at = 0; at < queue.size(); ++at) {
      const uint32_t lane = queue[at];
      key = std::min(key, static_cast<uint64_t>(ways[lane].FirstPoint) + 1);
      for (size_t next = neighbors.First[lane]; next < neighbors.First[lane + 1]; ++next) {
        const uint32_t joined = neighbors.Lanes[next];
        if (keys[joined] != 0) { continue; }
        keys[joined] = std::numeric_limits<uint64_t>::max();
        queue.push_back(joined);
      }
    }
    for (const uint32_t lane : queue) { keys[lane] = key; }
  }
  return groups;
}

}

void Corridors::GroupTerrainContacts(const Paving &on, Paved &into) {
  const auto began = std::chrono::steady_clock::now();
  const auto &ways = on.Ways.Ways();
  std::unordered_map<ContactNode, uint32_t, NodeHash> joined;
  joined.reserve(into.Edges.size() * 2);
  std::vector<Connection> connections;
  connections.reserve(into.Edges.size());
  for (const Edge &edge : into.Edges) {
    if (ways[edge.Lane].Bridge) { continue; }
    for (const uint64_t node : edge.NodeAt) {
      if (node == 0) { continue; }
      const auto [found, inserted] =
          joined.try_emplace(ContactNode{node, ways[edge.Lane].Layer}, edge.Lane);
      if (!inserted && found->second != edge.Lane) {
        connections.emplace_back(edge.Lane, found->second);
      }
    }
  }
  const auto neighbors = AlongConnections(ways.size(), connections);
  const size_t groups = AssignContactKeys(ways, neighbors, into.ContactKeys);
  Notes(into, "streets: terrain contact groups", static_cast<double>(groups), "groups");
  Notes(into,
        "streets: grouping terrain contacts",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
        "ms");
}

}
