#ifndef OUTSHINE_BASE_SPATIAL_PLANHIERARCHY_H
#define OUTSHINE_BASE_SPATIAL_PLANHIERARCHY_H

#include "math/Box.h"

#include <cstdint>
#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <vector>

namespace outshine {

enum class PlanHierarchyError { InvalidBounds, CapacityExceeded };

class PlanHierarchy {
public:
  struct Node {
    Box Bounds;
    uint32_t First = 0, Count = 0, Left = 0, Right = 0;
  };

  [[nodiscard]] static std::expected<PlanHierarchy, PlanHierarchyError>
  Build(std::span<const Box> bounds);

  [[nodiscard]] std::span<const Node> Nodes() const noexcept { return Nodes_; }

  [[nodiscard]] std::span<const uint32_t> Members(const Node &node) const noexcept {
    return std::span(Order_).subspan(node.First, node.Count);
  }

  template <class Accept, class Emit> void Select(Accept &&accept, Emit &&emit) const {
    if (Nodes_.empty()) { return; }
    std::array<uint32_t, 32> pending{};
    size_t count = 1;
    while (count != 0) {
      const Node &node = Nodes_[pending[--count]];
      if (node.Count == 1 || accept(node.Bounds)) {
        emit(node);
      } else {
        pending[count++] = node.Right;
        pending[count++] = node.Left;
      }
    }
  }

private:
  uint32_t Partition(std::span<const Box> bounds, uint32_t first, uint32_t count);
  std::vector<Node> Nodes_;
  std::vector<uint32_t> Order_;
};

}
#endif
