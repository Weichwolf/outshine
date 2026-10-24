#include "PlanHierarchy.h"

#include "Box.h"
#include "Vec3.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
namespace {
bool ValidBounds(const Box &box) {
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(box.Min[axis]) || !std::isfinite(box.Max[axis]) ||
        box.Min[axis] > box.Max[axis]) {
      return false;
    }
  }
  return true;
}

bool ValidStoredNode(std::span<const PlanHierarchy::Node> nodes,
                     std::span<const uint32_t> order,
                     std::span<const Box> bounds,
                     size_t at) {
  const auto &node = nodes[at];
  if (node.Count == 0 || node.First > bounds.size() || node.Count > bounds.size() - node.First ||
      !ValidBounds(node.Bounds)) {
    return false;
  }
  if (node.Count == 1) {
    const Box &source = bounds[order[node.First]];
    return node.Left == 0 && node.Right == 0 && node.Bounds.Holds(source.Min) &&
           node.Bounds.Holds(source.Max);
  }
  const auto leftCount = node.Count / 2;
  if (node.Left != at + 1 || node.Right != at + size_t{2} * leftCount ||
      node.Right >= nodes.size()) {
    return false;
  }
  const auto &left = nodes[node.Left];
  const auto &right = nodes[node.Right];
  return left.First == node.First && left.Count == leftCount &&
         right.First == node.First + leftCount && right.Count == node.Count - leftCount &&
         node.Bounds.Holds(left.Bounds.Min) && node.Bounds.Holds(left.Bounds.Max) &&
         node.Bounds.Holds(right.Bounds.Min) && node.Bounds.Holds(right.Bounds.Max);
}
}

bool PlanHierarchy::ValidRay(const Vec3 &origin,
                             const Vec3 &direction,
                             double minimum,
                             double maximum) noexcept {
  bool moving = false;
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(origin[axis]) || !std::isfinite(direction[axis])) { return false; }
    moving |= direction[axis] != 0.0;
  }
  return moving && std::isfinite(minimum) && minimum >= 0.0 && maximum >= minimum;
}

std::optional<double> PlanHierarchy::RayEntry(const Box &bounds,
                                              const Vec3 &origin,
                                              const Vec3 &direction,
                                              double minimum,
                                              double maximum) noexcept {
  for (size_t axis = 0; axis < 3; ++axis) {
    if (direction[axis] == 0.0) {
      if (origin[axis] < bounds.Min[axis] || origin[axis] > bounds.Max[axis]) {
        return std::nullopt;
      }
      continue;
    }
    const double first = (bounds.Min[axis] - origin[axis]) / direction[axis];
    const double last = (bounds.Max[axis] - origin[axis]) / direction[axis];
    minimum = std::max(minimum, std::min(first, last));
    maximum = std::min(maximum, std::max(first, last));
    if (minimum > maximum) { return std::nullopt; }
  }
  return minimum;
}

std::expected<PlanHierarchy, PlanHierarchyError> PlanHierarchy::Build(std::span<const Box> bounds) {
  if (bounds.size() > std::numeric_limits<uint32_t>::max() / 2u) {
    return std::unexpected(PlanHierarchyError::CapacityExceeded);
  }
  if (!std::ranges::all_of(bounds, ValidBounds)) {
    return std::unexpected(PlanHierarchyError::InvalidBounds);
  }
  PlanHierarchy hierarchy;
  hierarchy.Order_.resize(bounds.size());
  std::ranges::iota(hierarchy.Order_, uint32_t{0});
  if (!bounds.empty()) {
    hierarchy.Nodes_.reserve(bounds.size() * 2 - 1);
    (void)hierarchy.Partition(bounds, 0, static_cast<uint32_t>(bounds.size()));
  }
  return hierarchy;
}

std::expected<PlanHierarchy, PlanHierarchyError> PlanHierarchy::Restore(
    std::vector<Node> nodes, std::vector<uint32_t> sourceOrder, std::span<const Box> sourceBounds) {
  const auto sources = sourceBounds.size();
  if (sources > std::numeric_limits<uint32_t>::max() / 2u) {
    return std::unexpected(PlanHierarchyError::CapacityExceeded);
  }
  if (sourceOrder.size() != sources || nodes.size() != (sources == 0 ? 0 : sources * 2 - 1)) {
    return std::unexpected(PlanHierarchyError::InvalidStorage);
  }
  if (!std::ranges::all_of(sourceBounds, ValidBounds)) {
    return std::unexpected(PlanHierarchyError::InvalidBounds);
  }
  std::vector<uint8_t> seen(sources);
  for (const uint32_t source : sourceOrder) {
    if (source >= sources || seen[source] != 0) {
      return std::unexpected(PlanHierarchyError::InvalidStorage);
    }
    seen[source] = 1;
  }
  if (!nodes.empty() && (nodes.front().First != 0 || nodes.front().Count != sources)) {
    return std::unexpected(PlanHierarchyError::InvalidStorage);
  }
  for (size_t at = 0; at < nodes.size(); ++at) {
    if (!ValidStoredNode(nodes, sourceOrder, sourceBounds, at)) {
      return std::unexpected(PlanHierarchyError::InvalidStorage);
    }
  }
  PlanHierarchy hierarchy;
  hierarchy.Nodes_ = std::move(nodes);
  hierarchy.Order_ = std::move(sourceOrder);
  return hierarchy;
}

uint32_t PlanHierarchy::Partition(std::span<const Box> bounds, uint32_t first, uint32_t count) {
  const auto index = static_cast<uint32_t>(Nodes_.size());
  Node node{.Bounds = {}, .First = first, .Count = count};
  Box centres;
  for (uint32_t offset = first; offset < first + count; ++offset) {
    const Box &box = bounds[Order_[offset]];
    node.Bounds.Cover(box);
    Vec3 centre;
    for (size_t axis = 0; axis < 3; ++axis) {
      centre[axis] = std::midpoint(box.Min[axis], box.Max[axis]);
    }
    centres.Cover(centre);
  }
  Nodes_.push_back(node);
  if (count == 1) { return index; }
  const Vec3 span = centres.Span();
  size_t axis = 0;
  for (size_t next = 1; next < 3; ++next) {
    if (span[next] > span[axis]) { axis = next; }
  }
  const auto middle = first + count / 2;
  std::nth_element(Order_.begin() + first,
                   Order_.begin() + middle,
                   Order_.begin() + first + count,
                   [&](uint32_t left, uint32_t right) {
                     const double centreLeft =
                         std::midpoint(bounds[left].Min[axis], bounds[left].Max[axis]);
                     const double centreRight =
                         std::midpoint(bounds[right].Min[axis], bounds[right].Max[axis]);
                     return centreLeft != centreRight ? centreLeft < centreRight : left < right;
                   });
  const uint32_t left = Partition(bounds, first, middle - first);
  const uint32_t right = Partition(bounds, middle, first + count - middle);
  Nodes_[index].Left = left;
  Nodes_[index].Right = right;
  return index;
}

}
