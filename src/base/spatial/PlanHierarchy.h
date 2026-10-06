#ifndef OUTSHINE_BASE_SPATIAL_PLANHIERARCHY_H
#define OUTSHINE_BASE_SPATIAL_PLANHIERARCHY_H

#include "math/Box.h"
#include "math/Vec3.h"

#include <cstdint>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <utility>
#include <optional>
#include <span>
#include <vector>

namespace outshine {

enum class PlanHierarchyError { InvalidBounds, CapacityExceeded };

class PlanHierarchy {
public:
  struct RayHit {
    uint32_t Source = 0;
    double Along = 0.0;
  };

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

  template <class Accept, class Emit> void SelectNodes(Accept &&accept, Emit &&emit) const {
    if (Nodes_.empty()) { return; }
    std::array<uint32_t, 32> pending{};
    size_t count = 1;
    while (count != 0) {
      const Node &node = Nodes_[pending[--count]];
      if (node.Count == 1 || accept(node)) {
        emit(node);
      } else {
        pending[count++] = node.Right;
        pending[count++] = node.Left;
      }
    }
  }

  template <class Accept, class Emit> void Select(Accept &&accept, Emit &&emit) const {
    SelectNodes([&](const Node &node) { return accept(node.Bounds); }, emit);
  }

  template <class Intersect>
  [[nodiscard]] std::optional<RayHit> TraceClosest(const Vec3 &origin,
                                                   const Vec3 &direction,
                                                   double minimum,
                                                   double maximum,
                                                   Intersect &&intersect) const {
    if (Nodes_.empty() || !ValidRay(origin, direction, minimum, maximum)) { return std::nullopt; }
    const auto entry = RayEntry(Nodes_.front().Bounds, origin, direction, minimum, maximum);
    if (!entry) { return std::nullopt; }

    struct Pending {
      uint32_t Index;
      double Along;
    };

    std::array<Pending, 32> pending{};
    pending[0] = {.Index = 0, .Along = *entry};
    size_t count = 1;
    std::optional<RayHit> hit;
    while (count != 0) {
      const Pending next = pending[--count];
      if (next.Along > maximum) { continue; }
      const Node &node = Nodes_[next.Index];
      if (node.Count == 1) {
        const uint32_t source = Order_[node.First];
        const auto along = intersect(source, maximum);
        if (along && std::isfinite(*along) && *along >= minimum && *along <= maximum) {
          hit = RayHit{.Source = source, .Along = *along};
          maximum = *along;
        }
        continue;
      }
      const auto left = RayEntry(Nodes_[node.Left].Bounds, origin, direction, minimum, maximum);
      const auto right = RayEntry(Nodes_[node.Right].Bounds, origin, direction, minimum, maximum);
      const double missed = std::numeric_limits<double>::infinity();
      std::array children{Pending{.Index = node.Left, .Along = left.value_or(missed)},
                          Pending{.Index = node.Right, .Along = right.value_or(missed)}};
      if (children[0].Along < children[1].Along) { std::swap(children[0], children[1]); }
      for (const Pending child : children) {
        if (std::isfinite(child.Along) && child.Along <= maximum) { pending[count++] = child; }
      }
    }
    return hit;
  }

private:
  [[nodiscard]] static bool
  ValidRay(const Vec3 &origin, const Vec3 &direction, double minimum, double maximum) noexcept;
  [[nodiscard]] static std::optional<double> RayEntry(const Box &bounds,
                                                      const Vec3 &origin,
                                                      const Vec3 &direction,
                                                      double minimum,
                                                      double maximum) noexcept;
  uint32_t Partition(std::span<const Box> bounds, uint32_t first, uint32_t count);
  std::vector<Node> Nodes_;
  std::vector<uint32_t> Order_;
};

}
#endif
