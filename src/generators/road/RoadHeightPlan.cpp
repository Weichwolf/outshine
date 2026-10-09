#include "RoadHeightPlan.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <functional>
#include <numeric>
#include <queue>
#include <utility>

namespace outshine::Generators {
namespace {

struct Neighbor {
  uint32_t Node = 0;
  double RiseM = 0.0;
  double ReverseRiseM = 0.0;
};

struct Neighbors {
  std::vector<size_t> First;
  std::vector<Neighbor> Connected;
};

bool ValidNode(const RoadHeightNode &node) noexcept {
  return std::isfinite(node.LowSampleM) && std::isfinite(node.HighSampleM) &&
         node.LowSampleM <= node.HighSampleM && !std::isnan(node.MinimumM) &&
         !std::isnan(node.MaximumM) && node.MinimumM <= node.MaximumM &&
         node.MinimumM < std::numeric_limits<double>::infinity() &&
         node.MaximumM > -std::numeric_limits<double>::infinity();
}

Neighbors Connect(size_t nodes, std::span<const RoadHeightLink> links) {
  Neighbors out{.First = std::vector<size_t>(nodes + 1), .Connected = {}};
  for (const auto &link : links) {
    ++out.First[link.First + 1];
    ++out.First[link.Second + 1];
  }
  std::partial_sum(out.First.begin(), out.First.end(), out.First.begin());
  out.Connected.resize(out.First.back());
  auto next = out.First;
  for (const auto &link : links) {
    const double difference = link.FirstOffsetM - link.SecondOffsetM;
    out.Connected[next[link.First]++] = {
        link.Second, link.MaximumRiseM + difference, link.MaximumRiseM - difference};
    out.Connected[next[link.Second]++] = {
        link.First, link.MaximumRiseM - difference, link.MaximumRiseM + difference};
  }
  return out;
}

template <typename Seed>
std::expected<std::vector<double>, std::string_view> Envelope(std::span<const RoadHeightNode> nodes,
                                                              const Neighbors &neighbors,
                                                              Seed seed,
                                                              bool reverse) {
  using Visit = std::pair<double, uint32_t>;
  std::vector<double> heights;
  heights.reserve(nodes.size());
  std::vector<Visit> initial;
  initial.reserve(nodes.size());
  for (uint32_t at = 0; at < nodes.size(); ++at) {
    heights.push_back(seed(nodes[at]));
    if (std::isfinite(heights.back())) { initial.emplace_back(heights.back(), at); }
  }
  std::priority_queue<Visit, std::vector<Visit>, std::greater<>> frontier(std::greater<>{},
                                                                          std::move(initial));
  std::vector<size_t> pathEdges(nodes.size());
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  size_t visits = 0;
  while (!frontier.empty()) {
    if ((++visits % 4096) == 0 && std::chrono::steady_clock::now() >= deadline) {
      return std::unexpected("road height planning exceeded its work deadline");
    }
    const auto [height, node] = frontier.top();
    frontier.pop();
    if (height != heights[node]) { continue; }
    for (size_t at = neighbors.First[node]; at < neighbors.First[node + 1]; ++at) {
      const Neighbor &next = neighbors.Connected[at];
      const double candidate = height + (reverse ? next.ReverseRiseM : next.RiseM);
      if (candidate >= heights[next.Node]) { continue; }
      pathEdges[next.Node] = pathEdges[node] + 1;
      if (pathEdges[next.Node] >= nodes.size()) {
        return std::unexpected("road attachment offsets conflict with the permitted gradients");
      }
      heights[next.Node] = candidate;
      frontier.emplace(candidate, next.Node);
    }
  }
  return heights;
}

}

std::expected<RoadHeightPlan, std::string_view>
PlanRoadHeights(std::span<const RoadHeightNode> nodes, std::span<const RoadHeightLink> links) {
  if (nodes.size() > std::numeric_limits<uint32_t>::max() ||
      !std::ranges::all_of(nodes, ValidNode) ||
      !std::ranges::all_of(links,
                           [&](const auto &link) {
                             return link.First < nodes.size() && link.Second < nodes.size() &&
                                    std::isfinite(link.MaximumRiseM) && link.MaximumRiseM >= 0.0;
                           }) ||
      !std::ranges::all_of(links, [](const auto &link) {
        const double difference = link.FirstOffsetM - link.SecondOffsetM;
        return std::isfinite(difference) && std::isfinite(link.MaximumRiseM + difference) &&
               std::isfinite(link.MaximumRiseM - difference);
      })) {
    return std::unexpected("invalid road height constraints");
  }
  const auto neighbors = Connect(nodes.size(), links);
  const auto upper =
      Envelope(nodes, neighbors, [](const auto &node) { return node.LowSampleM; }, false);
  if (!upper) { return std::unexpected(upper.error()); }
  const auto lower =
      Envelope(nodes, neighbors, [](const auto &node) { return -node.HighSampleM; }, true);
  if (!lower) { return std::unexpected(lower.error()); }
  const auto floor =
      Envelope(nodes, neighbors, [](const auto &node) { return -node.MinimumM; }, true);
  if (!floor) { return std::unexpected(floor.error()); }
  const auto ceiling =
      Envelope(nodes, neighbors, [](const auto &node) { return node.MaximumM; }, false);
  if (!ceiling) { return std::unexpected(ceiling.error()); }
  RoadHeightPlan plan;
  for (size_t at = 0; at < nodes.size(); ++at) {
    if (-(*floor)[at] > (*ceiling)[at] + 1e-8) {
      return std::unexpected("road contacts conflict with the permitted gradients");
    }
    plan.MaximumAdjustmentM = std::max({plan.MaximumAdjustmentM,
                                        -(*lower)[at] * 0.5 - (*upper)[at] * 0.5,
                                        -(*floor)[at] - (*upper)[at],
                                        -(*lower)[at] - (*ceiling)[at]});
  }
  if (!std::isfinite(plan.MaximumAdjustmentM)) {
    return std::unexpected("road height adjustment exceeds its numeric range");
  }
  plan.HeightM.reserve(nodes.size());
  for (size_t at = 0; at < nodes.size(); ++at) {
    const double low = std::max(-(*floor)[at], -(*lower)[at] - plan.MaximumAdjustmentM);
    const double high = std::min((*ceiling)[at], (*upper)[at] + plan.MaximumAdjustmentM);
    const double reference = std::midpoint(-(*lower)[at], (*upper)[at]);
    plan.HeightM.push_back(std::clamp(reference, low, high));
  }
  return plan;
}

}
