#ifndef OUTSHINE_BASE_SPATIAL_HEIGHTSOLIDINTERNAL_H
#define OUTSHINE_BASE_SPATIAL_HEIGHTSOLIDINTERNAL_H

#include "HeightSolid.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::HeightSolids {
using Error = HeightSolidError;
template <typename T> using Checked = std::expected<T, Error>;

struct Incidence {
  uint32_t Face = 0;
  uint32_t Start = 0, End = 0;
};

struct Edge {
  std::array<Incidence, 2> Faces{};
  uint32_t Count = 0;
};

struct HeightNode {
  double Height = 0;
  uint32_t Vertex = 0;
};

struct State {
  const HeightSolidInput &Input;
  HeightSolidBudget Budget;
  const std::stop_token &Stop;
  HeightSolid Output;
  std::vector<PlanarFace> Bottom;
  std::vector<uint32_t> Bases;
  std::vector<size_t> Offsets;
  std::vector<HeightNode> Heights;
  std::vector<Edge> Edges;

  [[nodiscard]] Checked<void> Check() const;
  [[nodiscard]] Checked<void> VisitEdge();
};

[[nodiscard]] Checked<void>
ValidateInput(const HeightSolidInput &input, HeightSolidBudget budget, const std::stop_token &stop);
[[nodiscard]] Checked<void> AddWall(State &state,
                                    const std::array<uint32_t, 4> &corners,
                                    std::array<uint32_t, 2> bases,
                                    uint32_t parent,
                                    HeightSolidWallKind kind);
[[nodiscard]] Checked<void> CloseWalls(State &state);
[[nodiscard]] Checked<void> ValidateBody(State &state);
}
#endif
