#ifndef OUTSHINE_RENDER_DRAW_BATCHVISIBILITY_H
#define OUTSHINE_RENDER_DRAW_BATCHVISIBILITY_H

#include "FrameContext.h"
#include "ClusterDag.h"
#include "StoredVertex.h"
#include "Mat4.h"
#include "Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>

namespace outshine::Render {
using BatchSphere = std::array<float, 4>;

inline bool NormalOrZero(double value) {
  return std::isfinite(value) &&
         (value == 0 || std::abs(value) >= std::numeric_limits<float>::min());
}

inline std::optional<BatchSphere> PackSphere(const std::array<double, 3> &centre, double radius) {
  BatchSphere result{};
  std::array<double, 3> delta{};
  for (size_t axis = 0; axis < 3; ++axis) {
    result[axis] = static_cast<float>(centre[axis]);
    delta[axis] = result[axis] - centre[axis];
    if (!NormalOrZero(result[axis])) { return std::nullopt; }
  }
  result[3] = std::nextafter(static_cast<float>(radius + std::hypot(delta[0], delta[1], delta[2])),
                             std::numeric_limits<float>::infinity());
  if (!NormalOrZero(result[3]) || result[3] < 0) { return std::nullopt; }
  return result;
}

inline std::optional<BatchSphere> LocalSphereOf(std::span<const StoredVertex> vertices) {
  if (vertices.empty()) { return std::nullopt; }
  std::array<double, 3> lower{};
  std::array<double, 3> upper{};
  for (size_t axis = 0; axis < 3; ++axis) {
    lower[axis] = upper[axis] = vertices.front().pos[axis];
  }
  for (const auto &vertex : vertices) {
    for (size_t axis = 0; axis < 3; ++axis) {
      const double value = vertex.pos[axis];
      if (!NormalOrZero(value)) { return std::nullopt; }
      lower[axis] = std::min(lower[axis], value);
      upper[axis] = std::max(upper[axis], value);
    }
  }
  std::array<double, 3> centre{};
  std::array<double, 3> extent{};
  for (size_t axis = 0; axis < 3; ++axis) {
    centre[axis] = (lower[axis] + upper[axis]) / 2;
    extent[axis] = (upper[axis] - lower[axis]) / 2;
  }
  return PackSphere(centre, std::hypot(extent[0], extent[1], extent[2]));
}

inline std::optional<BatchSphere> LocalSphereOf(std::span<const DagCluster> clusters,
                                                size_t indexCount) {
  if (clusters.empty() || indexCount == 0 || indexCount % 3 != 0) { return std::nullopt; }
  std::array<double, 3> lower{};
  std::array<double, 3> upper{};
  lower.fill(std::numeric_limits<double>::infinity());
  upper.fill(-std::numeric_limits<double>::infinity());
  size_t covered = 0;
  for (const auto &cluster : clusters) {
    if (cluster.First > covered || cluster.First > indexCount || cluster.First % 3 != 0 ||
        cluster.Count == 0 || cluster.Count % 3 != 0 ||
        cluster.Count > indexCount - cluster.First || cluster.SelfRadius < 0 ||
        !NormalOrZero(cluster.SelfRadius)) {
      return std::nullopt;
    }
    covered = std::max(covered, size_t{cluster.First} + cluster.Count);
    for (size_t axis = 0; axis < 3; ++axis) {
      const double value = cluster.SelfCenter[axis];
      if (!NormalOrZero(value)) { return std::nullopt; }
      lower[axis] = std::min(lower[axis], value - cluster.SelfRadius);
      upper[axis] = std::max(upper[axis], value + cluster.SelfRadius);
    }
  }
  if (covered != indexCount) { return std::nullopt; }
  std::array<double, 3> centre{};
  for (size_t axis = 0; axis < 3; ++axis) { centre[axis] = (lower[axis] + upper[axis]) / 2; }
  double radius = 0;
  for (const auto &cluster : clusters) {
    radius = std::max(radius,
                      std::hypot(cluster.SelfCenter[0] - centre[0],
                                 cluster.SelfCenter[1] - centre[1],
                                 cluster.SelfCenter[2] - centre[2]) +
                          cluster.SelfRadius);
  }
  return PackSphere(centre, radius);
}

inline double SphereScaleUpperBound(const std::array<std::array<double, 4>, 3> &matrix) {
  double squaredScale = 0;
  for (size_t a = 0; a < 3; ++a) {
    double sum = 0;
    for (size_t b = 0; b < 3; ++b) {
      double dot = 0;
      for (size_t axis = 0; axis < 3; ++axis) { dot += matrix[axis][a] * matrix[axis][b]; }
      sum += std::abs(dot);
    }
    squaredScale = std::max(squaredScale, sum);
  }
  return std::sqrt(squaredScale);
}

class SphereTransform {
public:
  [[nodiscard]] static std::optional<SphereTransform> From(const Mat4 &row) {
    if (static_cast<float>(row.At(3, 3)) != 1) { return std::nullopt; }
    for (size_t axis = 0; axis < 3; ++axis) {
      if (static_cast<float>(row.At(3, axis)) != 0) { return std::nullopt; }
    }
    SphereTransform result;
    for (size_t axis = 0; axis < 3; ++axis) {
      for (size_t column = 0; column < 4; ++column) {
        result.Matrix_[axis][column] = static_cast<float>(row.At(axis, column));
        if (!NormalOrZero(result.Matrix_[axis][column])) { return std::nullopt; }
      }
    }
    result.Scale_ = SphereScaleUpperBound(result.Matrix_);
    return result;
  }

  [[nodiscard]] std::optional<BatchSphere> Apply(const BatchSphere &local) const {
    constexpr double kGuard = 8 * std::numeric_limits<float>::epsilon();
    std::array<double, 3> centre{};
    std::array<double, 3> error{};
    for (size_t axis = 0; axis < 3; ++axis) {
      centre[axis] = Matrix_[axis][3];
      error[axis] = std::abs(Matrix_[axis][3]) + 1;
      for (size_t column = 0; column < 3; ++column) {
        centre[axis] += Matrix_[axis][column] * local[column];
        error[axis] += std::abs(Matrix_[axis][column]) * (std::abs(local[column]) + local[3]);
      }
      if (error[axis] > std::numeric_limits<float>::max()) { return std::nullopt; }
      error[axis] *= kGuard;
    }
    const double radius =
        local[3] * Scale_ * (1 + kGuard) + std::hypot(error[0], error[1], error[2]);
    return PackSphere(centre, radius);
  }

private:
  SphereTransform() = default;
  std::array<std::array<double, 4>, 3> Matrix_{};
  double Scale_ = 0;
};

inline std::optional<BatchSphere> TransformedSphere(const BatchSphere &local, const Mat4 &row) {
  const auto transform = SphereTransform::From(row);
  return transform ? transform->Apply(local) : std::nullopt;
}

inline std::optional<BatchSphere> WorldSphereOf(const std::optional<BatchSphere> &local,
                                                std::span<const Mat4> rows) {
  if (!local || rows.empty()) { return std::nullopt; }
  if (rows.size() == 1) { return TransformedSphere(*local, rows.front()); }
  std::array<double, 3> lower{};
  std::array<double, 3> upper{};
  lower.fill(std::numeric_limits<double>::infinity());
  upper.fill(-std::numeric_limits<double>::infinity());
  for (const auto &row : rows) {
    const auto sphere = TransformedSphere(*local, row);
    if (!sphere) { return std::nullopt; }
    for (size_t axis = 0; axis < 3; ++axis) {
      lower[axis] = std::min(lower[axis], static_cast<double>((*sphere)[axis]) - (*sphere)[3]);
      upper[axis] = std::max(upper[axis], static_cast<double>((*sphere)[axis]) + (*sphere)[3]);
    }
  }
  std::array<double, 3> centre{};
  std::array<double, 3> extent{};
  for (size_t axis = 0; axis < 3; ++axis) {
    centre[axis] = (lower[axis] + upper[axis]) / 2;
    extent[axis] = (upper[axis] - lower[axis]) / 2;
  }
  return PackSphere(centre, std::hypot(extent[0], extent[1], extent[2]));
}

class BatchFrustum {
public:
  BatchFrustum(const FrameContext &frame, const Vec3 &anchor) {
    for (size_t axis = 0; axis < 3; ++axis) {
      Shift_[axis] = static_cast<float>(anchor[axis] + frame.PreViewTranslation[axis]);
      Valid_ &= NormalOrZero(Shift_[axis]);
    }
    for (size_t side = 0; side < Planes_.size(); ++side) {
      auto &plane = Planes_[side];
      const double sign = side % 2 == 0 ? 1 : -1;
      for (size_t column = 0; column < 4; ++column) {
        const double a = frame.Mvp.At(3, column);
        const double b = frame.Mvp.At(side / 2, column);
        Valid_ &= NormalOrZero(a) && NormalOrZero(b);
        plane.Equation[column] = a + sign * b;
        plane.Error[column] = std::abs(a) + std::abs(b);
      }
      plane.Scale = std::hypot(plane.Equation[0], plane.Equation[1], plane.Equation[2]);
    }
  }

  [[nodiscard]] bool Outside(const std::optional<BatchSphere> &sphere) const {
    if (!Valid_ || !sphere || (*sphere)[3] < 0 || !std::ranges::all_of(*sphere, NormalOrZero)) {
      return false;
    }
    constexpr double kGuard = 32 * std::numeric_limits<float>::epsilon();
    bool outside = false;
    for (const auto &plane : Planes_) {
      double maximum = plane.Equation[3] + (*sphere)[3] * plane.Scale;
      double uncertainty = plane.Error[3] + 1;
      for (size_t axis = 0; axis < 3; ++axis) {
        maximum += plane.Equation[axis] * ((*sphere)[axis] + Shift_[axis]);
        uncertainty +=
            plane.Error[axis] * (std::abs((*sphere)[axis]) + (*sphere)[3] + std::abs(Shift_[axis]));
      }
      if (uncertainty > std::numeric_limits<float>::max()) { return false; }
      outside |= maximum < -kGuard * uncertainty;
    }
    return outside;
  }

private:
  struct Plane {
    std::array<double, 4> Equation{}, Error{};
    double Scale = 0;
  };

  std::array<Plane, 4> Planes_{};
  std::array<double, 3> Shift_{};
  bool Valid_ = true;
};
}
#endif
