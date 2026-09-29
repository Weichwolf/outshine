#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTURESURFACEINDEX_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTURESURFACEINDEX_H

#include "StructureSurfaceError.h"
#include "TriangleRegion.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>
#include <limits>

namespace outshine::Generators {

class StructureSurfaceIndex {
public:
  struct Node {
    Vec3f MinM{};
    Vec3f MaxM{};
    uint32_t First = 0;
    uint32_t Count = 0;
    uint32_t Left = 0;
    uint32_t Right = 0;
  };

  struct Match {
    uint64_t Key = 0;
    size_t Slot = 0;
    size_t Remaining = 64;
  };

  [[nodiscard]] Match MatchOf(const std::array<PointEnclosure, 3> &vertices) const noexcept;
  [[nodiscard]] std::optional<size_t> NextMatch(Match &match) const noexcept;
  void Reset(const Raised &validated);
  [[nodiscard]] bool Step();

  [[nodiscard]] size_t NodeCount() const noexcept { return Nodes_.size(); }

  [[nodiscard]] const Node &At(size_t node) const noexcept { return Nodes_[node]; }

  [[nodiscard]] std::array<PointEnclosure, 3> Triangle(size_t triangle) const noexcept;
  [[nodiscard]] double LowerDistance(size_t node, Vec3 point) const noexcept;

  [[nodiscard]] size_t CapacityBytes() const noexcept {
    return Nodes_.capacity() * sizeof(Node) + Matches_.capacity() * sizeof(Entry);
  }

private:
  struct Entry {
    uint64_t Key = 0;
    uint32_t Triangle = std::numeric_limits<uint32_t>::max();
  };

  std::vector<Entry> Matches_;
  size_t MatchSlots_ = 0;
  std::optional<uint32_t> Inserting_;
  Match Insertion_;
  const Raised *Input_ = nullptr;
  std::vector<Node> Nodes_;
  size_t Cursor_ = 0;
  size_t Folding_ = 0;
};

}
#endif
