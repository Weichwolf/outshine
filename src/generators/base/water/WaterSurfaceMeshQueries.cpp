#include "WaterSurfaceMesh.h"
#include "Geodesy.h"
#include "math/Box.h"
#include "math/Vec3.h"
#include "math/Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace outshine::Generators {
namespace {
struct QueryRay {
  Vec3 Origin, Direction;
};

Vec3 Vector(EastNorthUp point) {
  return {{point.EastM, point.NorthM, point.UpM}};
}

std::optional<double> Intersect(const std::array<Vec3, 3> &triangle, const QueryRay &ray) {
  const auto &direction = ray.Direction;
  size_t major = 0;
  for (size_t axis = 1; axis < 3; ++axis) {
    if (std::abs(direction[axis]) > std::abs(direction[major])) { major = axis; }
  }
  const size_t x = (major + 1) % 3;
  const size_t y = (x + 1) % 3;
  const double shearX = direction[x] / direction[major];
  const double shearY = direction[y] / direction[major];
  std::array<Vec3, 3> projected;
  for (size_t at = 0; at < 3; ++at) {
    const Vec3 delta = triangle[at] - ray.Origin;
    projected[at] = {{delta[x] - shearX * delta[major],
                      delta[y] - shearY * delta[major],
                      delta[major] / direction[major]}};
  }
  const auto edge = [](const Vec3 &a, const Vec3 &b) { return a[0] * b[1] - a[1] * b[0]; };
  const double u = edge(projected[1], projected[2]);
  const double v = edge(projected[2], projected[0]);
  const double w = edge(projected[0], projected[1]);
  if ((u < 0 || v < 0 || w < 0) && (u > 0 || v > 0 || w > 0)) { return std::nullopt; }
  const double determinant = u + v + w;
  if (determinant == 0) { return std::nullopt; }
  const double along =
      (u * projected[0][2] + v * projected[1][2] + w * projected[2][2]) / determinant;
  return std::isfinite(along) && along >= 0 ? std::optional(along) : std::nullopt;
}

double HeightAbove(const Box &box, const Vec3 &sea, const Vec3 &up) {
  double height = 0;
  double error = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    height +=
        std::max((box.Min[axis] - sea[axis]) * up[axis], (box.Max[axis] - sea[axis]) * up[axis]);
    error += std::abs(sea[axis]) + std::max(std::abs(box.Min[axis]), std::abs(box.Max[axis]));
  }
  return (height + 32 * std::numeric_limits<double>::epsilon() * error) / Dot(up, up);
}
}

std::optional<double> WaterSurfaceMesh::LevelAt(LongitudeLatitude at) const noexcept {
  if (Hierarchy_.Nodes().empty() || !std::isfinite(at.LongitudeDeg) ||
      !std::isfinite(at.LatitudeDeg) || std::abs(at.LongitudeDeg) > kDegPerHalfTurn ||
      std::abs(at.LatitudeDeg) > kDegPerHalfTurn / 2) {
    return std::nullopt;
  }
  const LongitudeLatitudeHeight geographic{
      .LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = 0};
  const Vec3 sea = Vector(Frame_.ToLocalPosition(geographic));
  const Vec3 up = Vector(Frame_.ToLocalDirection(EnuAxesEcef(geographic).Up));
  if (up[2] <= 0) { return std::nullopt; }
  const double height = HeightAbove(Hierarchy_.Nodes().front().Bounds, sea, up);
  const QueryRay ray{.Origin = sea + up * height, .Direction = up * -1.0};
  const auto hit =
      Hierarchy_.TraceClosest(ray.Origin,
                              ray.Direction,
                              0,
                              std::numeric_limits<double>::infinity(),
                              [&](uint32_t index, double maximum) -> std::optional<double> {
                                const auto &face = Faces_[index];
                                const std::array triangle{Vertices_[face.Vertices[0]],
                                                          Vertices_[face.Vertices[1]],
                                                          Vertices_[face.Vertices[2]]};
                                const auto along = Intersect(triangle, ray);
                                return along && *along <= maximum ? along : std::nullopt;
                              });
  return hit ? std::optional(height - hit->Along) : std::nullopt;
}
}
