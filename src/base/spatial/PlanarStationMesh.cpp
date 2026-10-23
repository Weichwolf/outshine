#include "PlanarStationMeshInternal.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::PlanarStations {
double Area(PlanarPoint a, PlanarPoint b, PlanarPoint c) {
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

namespace {
bool Finite(PlanarPoint point) {
  return std::isfinite(point[0]) && std::isfinite(point[1]);
}

Checked<void> ValidateAxes(std::span<const PlanarStationAxis> axes, size_t maximumCuts) {
  size_t cuts = 0;
  for (const auto &axis : axes) {
    if (!Finite(axis.Origin) || !Finite(axis.Direction) ||
        (axis.Direction[0] == 0 && axis.Direction[1] == 0)) {
      return std::unexpected(Error::InvalidInput);
    }
    if (axis.Cuts.size() > maximumCuts - cuts) { return std::unexpected(Error::CapacityExceeded); }
    cuts += axis.Cuts.size();
    for (const double station : axis.Cuts) {
      if (!std::isfinite(station)) { return std::unexpected(Error::InvalidInput); }
    }
  }
  return {};
}

Checked<void> Validate(std::span<const PlanarPoint> points,
                       std::span<const PlanarFace> faces,
                       std::span<const PlanarStationAxis> axes,
                       PlanarStationBudget budget,
                       const std::stop_token &stop) {
  constexpr size_t indexLimit = std::numeric_limits<uint32_t>::max();
  if (points.size() > budget.Vertices || faces.size() > budget.Faces ||
      points.size() > indexLimit || faces.size() > indexLimit || axes.size() > budget.Axes) {
    return std::unexpected(Error::CapacityExceeded);
  }
  for (const auto point : points) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!Finite(point)) { return std::unexpected(Error::InvalidInput); }
  }
  for (const auto face : faces) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    for (const auto vertex : face) {
      if (vertex >= points.size()) { return std::unexpected(Error::InvalidInput); }
    }
    const double area = Area(points[face[0]], points[face[1]], points[face[2]]);
    if (!std::isfinite(area) || area <= 0) { return std::unexpected(Error::InvalidInput); }
  }
  return ValidateAxes(axes, budget.Cuts);
}

Checked<PlanarStationMesh> Prepare(std::span<const PlanarPoint> points,
                                   std::span<const PlanarFace> faces,
                                   std::span<const PlanarStationAxis> axes) {
  PlanarStationMesh output;
  auto &mesh = output.Mesh;
  mesh.Points.assign(points.begin(), points.end());
  mesh.Faces.assign(faces.begin(), faces.end());
  mesh.PointRemap.resize(points.size());
  mesh.FaceParents.resize(faces.size());
  std::ranges::iota(mesh.PointRemap, 0u);
  std::ranges::iota(mesh.FaceParents, 0u);
  output.Stations.resize(axes.size());
  for (size_t at = 0; at < axes.size(); ++at) {
    const auto &axis = axes[at];
    auto &stations = output.Stations[at];
    stations.reserve(points.size());
    for (const auto point : points) {
      const double station = (point[0] - axis.Origin[0]) * axis.Direction[0] +
                             (point[1] - axis.Origin[1]) * axis.Direction[1];
      if (!std::isfinite(station)) { return std::unexpected(Error::InvalidInput); }
      stations.push_back(station);
    }
  }
  return output;
}
}
}

namespace outshine {
std::expected<PlanarStationMesh, PlanarMeshError>
CutPlanarStations(std::span<const PlanarPoint> points,
                  std::span<const PlanarFace> faces,
                  std::span<const PlanarStationAxis> axes,
                  PlanarStationBudget budget,
                  const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(PlanarMeshError::Canceled); }
  if (auto valid = PlanarStations::Validate(points, faces, axes, budget, stop); !valid) {
    return std::unexpected(valid.error());
  }
  auto output = PlanarStations::Prepare(points, faces, axes);
  if (!output) { return output; }
  for (size_t axis = 0; axis < axes.size(); ++axis) {
    for (const double cut : axes[axis].Cuts) {
      if (auto result = PlanarStations::Cut(*output, axis, cut, budget, stop); !result) {
        return std::unexpected(result.error());
      }
    }
  }
  return output;
}
}
