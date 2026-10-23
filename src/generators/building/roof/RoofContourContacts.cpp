#include "RoofContourInternal.h"
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <map>
#include <numbers>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
namespace {
struct Contacts {
  Ring Points;
  std::vector<size_t> Starts, Ends, Owners;
  std::vector<std::vector<size_t>> Incoming, Outgoing;
};

Checked<Contacts> IndexContacts(std::span<const Loop> loops, double tolerance) {
  Ring input;
  for (const auto &loop : loops) {
    input.insert(input.end(), loop.Points.begin(), loop.Points.end());
  }
  const auto shared = WeldVertices(input, tolerance);
  if (!shared) { return std::unexpected(shared.error()); }
  Contacts graph;
  graph.Points = Unique(*shared);
  std::map<Point, size_t> labels;
  for (size_t at = 0; at < graph.Points.size(); ++at) { labels.emplace(graph.Points[at], at); }
  size_t offset = 0;
  for (const auto &loop : loops) {
    const size_t count = loop.Points.size();
    for (size_t at = 0; at < count; ++at) {
      graph.Starts.push_back(labels.at((*shared)[offset + at]));
      graph.Ends.push_back(labels.at((*shared)[offset + (at + 1) % count]));
      graph.Owners.push_back(loop.Owners[at]);
    }
    offset += count;
  }
  graph.Incoming.resize(graph.Points.size());
  graph.Outgoing.resize(graph.Points.size());
  for (size_t at = 0; at < graph.Starts.size(); ++at) {
    graph.Outgoing[graph.Starts[at]].push_back(at);
    graph.Incoming[graph.Ends[at]].push_back(at);
  }
  for (size_t at = 0; at < graph.Points.size(); ++at) {
    if (graph.Incoming[at].size() != graph.Outgoing[at].size() || graph.Outgoing[at].empty()) {
      return std::unexpected(Failure{"unbalanced contact"});
    }
  }
  return graph;
}

size_t ClockwiseSuccessor(const Contacts &graph, size_t edge) {
  const size_t node = graph.Ends[edge];
  const auto back = Subtract(graph.Points[graph.Starts[edge]], graph.Points[node]);
  const double angle = std::atan2(back[1], back[0]);
  double best = std::numeric_limits<double>::infinity();
  size_t chosen = 0;
  for (const auto target : graph.Outgoing[node]) {
    const auto direction = Subtract(graph.Points[graph.Ends[target]], graph.Points[node]);
    double turn = std::fmod(angle - std::atan2(direction[1], direction[0]), 2 * std::numbers::pi);
    if (turn < 0) { turn += 2 * std::numbers::pi; }
    if (turn < best) {
      best = turn;
      chosen = target;
    }
  }
  return chosen;
}

Checked<void> PairIncoming(const Contacts &graph, size_t node, std::span<size_t> following) {
  std::set<size_t> selected;
  for (const auto edge : graph.Incoming[node]) {
    const size_t chosen = ClockwiseSuccessor(graph, edge);
    if (!selected.insert(chosen).second) {
      return std::unexpected(Failure{"ambiguous directed contact"});
    }
    following[edge] = chosen;
  }
  return {};
}

Checked<std::vector<size_t>> LinkContacts(const Contacts &graph) {
  std::vector<size_t> following;
  following.reserve(graph.Starts.size());
  for (const auto end : graph.Ends) { following.push_back(graph.Outgoing[end].back()); }
  for (size_t node = 0; node < graph.Points.size(); ++node) {
    if (graph.Outgoing[node].size() < 2) { continue; }
    const auto paired = PairIncoming(graph, node, following);
    if (!paired) { return std::unexpected(paired.error()); }
  }
  return following;
}

Checked<Loop> SourceCorners(const Contacts &graph, std::span<const size_t> edges) {
  Loop output;
  output.Points.reserve(edges.size());
  output.Owners.reserve(edges.size());
  for (size_t at = 0; at < edges.size(); ++at) {
    const auto edge = edges[at];
    if (graph.Owners[edge] == graph.Owners[edges[(at + edges.size() - 1) % edges.size()]]) {
      continue;
    }
    output.Owners.push_back(graph.Owners[edge]);
    output.Points.push_back(graph.Points[graph.Starts[edge]]);
  }
  if (output.Points.size() < 3) {
    return std::unexpected(Failure{"fewer than three source corners"});
  }
  return output;
}

Checked<std::vector<Loop>> WalkContacts(const Contacts &graph, std::span<const size_t> following) {
  std::vector<bool> used(graph.Starts.size());
  std::vector<Loop> result;
  for (size_t first = 0; first < graph.Starts.size(); ++first) {
    if (used[first]) { continue; }
    std::vector<size_t> ring;
    size_t edge = first;
    while (!used[edge]) {
      used[edge] = true;
      ring.push_back(edge);
      edge = following[edge];
    }
    if (edge != first) { return std::unexpected(Failure{"contact cycle joined another ring"}); }
    auto output = SourceCorners(graph, ring);
    if (!output) { return std::unexpected(output.error()); }
    result.push_back(std::move(*output));
  }
  return result;
}
}

Checked<std::vector<Loop>> Stitch(std::span<const Loop> loops, double tolerance) {
  const auto graph = IndexContacts(loops, tolerance);
  if (!graph) { return std::unexpected(graph.error()); }
  const auto following = LinkContacts(*graph);
  if (!following) { return std::unexpected(following.error()); }
  return WalkContacts(*graph, *following);
}
}
