#include "PlanHierarchy.h"

#include "math/Box.h"
#include "math/Vec3.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <optional>
#include <span>

namespace outshine {

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
  for (const Box &box : bounds) {
    for (size_t axis = 0; axis < 3; ++axis) {
      if (!std::isfinite(box.Min[axis]) || !std::isfinite(box.Max[axis]) ||
          box.Min[axis] > box.Max[axis]) {
        return std::unexpected(PlanHierarchyError::InvalidBounds);
      }
    }
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
