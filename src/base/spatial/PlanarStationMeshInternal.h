#ifndef OUTSHINE_BASE_SPATIAL_PLANARSTATIONMESHINTERNAL_H
#define OUTSHINE_BASE_SPATIAL_PLANARSTATIONMESHINTERNAL_H

#include "PlanarStationMesh.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <unordered_map>
#include <vector>

namespace outshine::PlanarStations {
using Error = PlanarMeshError;
template <typename T> using Checked = std::expected<T, Error>;
using SourceEdge = std::array<uint32_t, 2>;

struct CutState {
  PlanarStationMesh &Output;
  size_t Axis;
  double Station;
  PlanarStationBudget Budget;
  std::vector<SourceEdge> &Roots;
  std::unordered_map<uint64_t, uint32_t> Edges;
  std::vector<PlanarFace> Faces;
  std::vector<uint32_t> Parents;
  std::vector<uint32_t> Remaining;
};

[[nodiscard]] double Area(PlanarPoint a, PlanarPoint b, PlanarPoint c);
[[nodiscard]] Checked<void> Validate(std::span<const PlanarPoint> points,
                                     std::span<const PlanarFace> faces,
                                     std::span<const PlanarStationAxis> axes,
                                     PlanarStationBudget budget,
                                     const std::stop_token &stop);
[[nodiscard]] Checked<PlanarStationMesh> Prepare(std::span<const PlanarPoint> points,
                                                 std::span<const PlanarFace> faces,
                                                 std::span<const PlanarStationAxis> axes);
[[nodiscard]] Checked<uint32_t> Crossing(CutState &state, uint32_t a, uint32_t b);
[[nodiscard]] Checked<void> Emit(CutState &state, PlanarFace face, uint32_t parent);
[[nodiscard]] Checked<void> Conform(CutState &state, PlanarFace face, uint32_t parent);
[[nodiscard]] Checked<void> Cut(PlanarStationMesh &output,
                                size_t axis,
                                double station,
                                std::optional<std::span<const uint8_t>> selectedParents,
                                std::vector<SourceEdge> &roots,
                                PlanarStationBudget budget,
                                const std::stop_token &stop);
}
#endif
