#include "ClipmapTopology.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
struct TopologyPlan {
  uint32_t Side;
  size_t Vertices;
  size_t Indices;
  size_t Scratch;
};

std::expected<TopologyPlan, ClipmapTopologyError> Plan(ClipmapShape shape, size_t bytesMost) {
  const uint64_t half = shape.HalfCells;
  const uint64_t side = 2 * half + 1;
  if (half < 8 || (half & 1u) != 0 || side > std::numeric_limits<uint32_t>::max() / side ||
      (shape.HolePhase && ((*shape.HolePhase)[0] > 1 || (*shape.HolePhase)[1] > 1))) {
    return std::unexpected(ClipmapTopologyError::InvalidShape);
  }
  const uint64_t vertices = shape.HolePhase ? 3 * half * half + 10 * half : side * side;
  const uint64_t faces = shape.HolePhase ? 6 * half * half + 4 * half : 8 * half * half;
  const uint64_t scratch = side * side;
  const uint64_t bytes =
      vertices * sizeof(ClipmapVertex) + (3 * faces + scratch) * sizeof(uint32_t);
  if (bytes > bytesMost) { return std::unexpected(ClipmapTopologyError::OverBudget); }
  return TopologyPlan{.Side = static_cast<uint32_t>(side),
                      .Vertices = static_cast<size_t>(vertices),
                      .Indices = static_cast<size_t>(3 * faces),
                      .Scratch = static_cast<size_t>(scratch)};
}

enum class CellEdge { None = -1, South, East, North, West };

class Builder {
public:
  Builder(TopologyPlan plan, uint32_t half) : Side_(plan.Side), Half_(half) {
    Mesh_.Vertices.reserve(plan.Vertices);
    Mesh_.Indices.reserve(plan.Indices);
    Nodes_.resize(plan.Scratch, std::numeric_limits<uint32_t>::max());
  }

  void Cell(uint32_t x, uint32_t y, CellEdge edge) {
    const std::array corners{Node(x, y), Node(x + 1, y), Node(x + 1, y + 1), Node(x, y + 1)};
    if (edge == CellEdge::None) {
      Triangle(corners[0], corners[1], corners[2]);
      Triangle(corners[0], corners[2], corners[3]);
      return;
    }
    const auto at = [&corners, edge](size_t offset) {
      return corners[(static_cast<size_t>(edge) + offset) % corners.size()];
    };
    const auto a = Mesh_.Vertices[at(0)];
    const auto b = Mesh_.Vertices[at(1)];
    const auto middle = static_cast<uint32_t>(Mesh_.Vertices.size());
    Mesh_.Vertices.push_back(
        {.East = (a.East + b.East) * 0.5f, .North = (a.North + b.North) * 0.5f});
    Triangle(at(3), at(0), middle);
    Triangle(at(3), middle, at(1));
    Triangle(at(3), at(1), at(2));
  }

  [[nodiscard]] ClipmapTopology Take() && { return std::move(Mesh_); }

private:
  ClipmapTopology Mesh_;

  uint32_t Node(uint32_t x, uint32_t y) {
    auto &index = Nodes_[static_cast<size_t>(y) * Side_ + x];
    if (index == std::numeric_limits<uint32_t>::max()) {
      index = static_cast<uint32_t>(Mesh_.Vertices.size());
      Mesh_.Vertices.push_back({.East = static_cast<float>(x) - static_cast<float>(Half_),
                                .North = static_cast<float>(y) - static_cast<float>(Half_)});
    }
    return index;
  }

  void Triangle(uint32_t a, uint32_t b, uint32_t c) {
    Mesh_.Indices.insert(Mesh_.Indices.end(), {a, b, c});
  }

  uint32_t Side_;
  uint32_t Half_;
  std::vector<uint32_t> Nodes_;
};

struct Hole {
  uint32_t West, South, East, North;

  [[nodiscard]] bool Contains(uint32_t x, uint32_t y) const {
    return x >= West && x < East && y >= South && y < North;
  }

  [[nodiscard]] CellEdge StitchAt(uint32_t x, uint32_t y) const {
    if (y == North && x >= West && x < East) { return CellEdge::South; }
    if (x + 1 == West && y >= South && y < North) { return CellEdge::East; }
    if (y + 1 == South && x >= West && x < East) { return CellEdge::North; }
    if (x == East && y >= South && y < North) { return CellEdge::West; }
    return CellEdge::None;
  }
};
}

std::expected<ClipmapTopology, ClipmapTopologyError> ClipmapTopology::Make(ClipmapShape shape,
                                                                           size_t bytesMost) {
  const auto plan = Plan(shape, bytesMost);
  if (!plan) { return std::unexpected(plan.error()); }
  std::optional<Hole> hole;
  if (shape.HolePhase) {
    const uint32_t west = shape.HalfCells / 2 + (*shape.HolePhase)[0];
    const uint32_t south = shape.HalfCells / 2 + (*shape.HolePhase)[1];
    hole = Hole{.West = west,
                .South = south,
                .East = west + shape.HalfCells,
                .North = south + shape.HalfCells};
  }
  Builder builder(*plan, shape.HalfCells);
  for (uint32_t y = 0; y < 2 * shape.HalfCells; ++y) {
    for (uint32_t x = 0; x < 2 * shape.HalfCells; ++x) {
      if (hole && hole->Contains(x, y)) { continue; }
      builder.Cell(x, y, hole ? hole->StitchAt(x, y) : CellEdge::None);
    }
  }
  return std::move(builder).Take();
}
}
