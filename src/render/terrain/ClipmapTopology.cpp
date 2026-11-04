#include "ClipmapTopology.h"
#include <algorithm>
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
  uint32_t BlockCells;
  size_t Blocks;
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
  const uint32_t blockCells = std::max(2u, shape.HalfCells / 4u);
  const uint64_t columns = (2 * half + blockCells - 1) / blockCells;
  const uint64_t bytes = vertices * sizeof(ClipmapVertex) +
                         (3 * faces + scratch) * sizeof(uint32_t) +
                         columns * columns * sizeof(ClipmapBlock);
  if (bytes > bytesMost) { return std::unexpected(ClipmapTopologyError::OverBudget); }
  return TopologyPlan{.Side = static_cast<uint32_t>(side),
                      .Vertices = static_cast<size_t>(vertices),
                      .Indices = static_cast<size_t>(3 * faces),
                      .Scratch = static_cast<size_t>(scratch),
                      .BlockCells = blockCells,
                      .Blocks = static_cast<size_t>(columns * columns)};
}

enum class CellEdge { None = -1, South, East, North, West };

class Builder {
public:
  Builder(TopologyPlan plan, uint32_t half) : Side_(plan.Side), Half_(half) {
    Mesh_.Vertices.reserve(plan.Vertices);
    Mesh_.Indices.reserve(plan.Indices);
    Mesh_.Blocks.reserve(plan.Blocks);
    Nodes_.resize(plan.Scratch, std::numeric_limits<uint32_t>::max());
  }

  void BeginBlock(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    const auto half = static_cast<float>(Half_);
    Mesh_.Blocks.push_back(
        {.First = static_cast<uint32_t>(Mesh_.Indices.size()),
         .Low = {static_cast<float>(x) - half, static_cast<float>(y) - half},
         .High = {static_cast<float>(x + width) - half, static_cast<float>(y + height) - half}});
  }

  void EndBlock() {
    auto &block = Mesh_.Blocks.back();
    block.Count = static_cast<uint32_t>(Mesh_.Indices.size()) - block.First;
    if (block.Count == 0) { Mesh_.Blocks.pop_back(); }
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
  const uint32_t side = 2 * shape.HalfCells;
  for (uint32_t by = 0; by < side; by += plan->BlockCells) {
    const uint32_t height = std::min(plan->BlockCells, side - by);
    for (uint32_t bx = 0; bx < side; bx += plan->BlockCells) {
      const uint32_t width = std::min(plan->BlockCells, side - bx);
      builder.BeginBlock(bx, by, width, height);
      for (uint32_t y = by; y < by + height; ++y) {
        for (uint32_t x = bx; x < bx + width; ++x) {
          if (hole && hole->Contains(x, y)) { continue; }
          builder.Cell(x, y, hole ? hole->StitchAt(x, y) : CellEdge::None);
        }
      }
      builder.EndBlock();
    }
  }
  return std::move(builder).Take();
}

std::expected<ClipmapTopologySize, ClipmapTopologyError>
ClipmapTopology::Measure(ClipmapShape shape) {
  const auto plan = Plan(shape, std::numeric_limits<size_t>::max());
  if (!plan) { return std::unexpected(plan.error()); }
  return ClipmapTopologySize{.VertexBytes = plan->Vertices * sizeof(ClipmapVertex),
                             .IndexBytes = plan->Indices * sizeof(uint32_t),
                             .BlockBytes = plan->Blocks * sizeof(ClipmapBlock),
                             .ScratchBytes = plan->Scratch * sizeof(uint32_t)};
}
}
