#include "FixedPatch.h"
#include "Exact.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <utility>
#include <vector>

namespace FixedCoverage {
Exact Edge(const ExactPoint &a, const ExactPoint &b, const ExactPoint &point) {
  return (b.X - a.X) * (point.Y - a.Y) - (b.Y - a.Y) * (point.X - a.X);
}

bool Inside(const std::array<ExactPoint, 4> &corners, const ExactPoint &point) {
  for (size_t i = 0; i < corners.size(); ++i) {
    if (Edge(corners[i], corners[(i + 1) % corners.size()], point).Sign() < 0) { return false; }
  }
  return true;
}

std::expected<Projected, Unknown>
Project(const SourceQuad &source, uint32_t width, uint32_t height) {
  if (!source.Opaque) { return std::unexpected(Unknown::NonOpaque); }
  if (!source.Complete) { return std::unexpected(Unknown::Incomplete); }
  if (width == 0 || height == 0 || width > MaximumCoordinate || height > MaximumCoordinate) {
    return std::unexpected(Unknown::InvalidProfile);
  }
  Projected result;
  result.FarDepth = Exact(1);
  for (size_t i = 0; i < source.Points.size(); ++i) {
    const auto &point = source.Points[i];
    for (const auto component : point) {
      if (!std::isfinite(component)) { return std::unexpected(Unknown::Nonfinite); }
    }
    const Exact divisor = Exact::Float32(point[3]);
    const Exact z = Exact::Float32(point[2]);
    if (divisor.Sign() <= 0 || z.Sign() < 0 || divisor < z) {
      return std::unexpected(Unknown::ClipCrossing);
    }
    result.Corners[i] = {
        .X = (Exact::Float32(point[0]) / divisor + Exact(1)) * Exact(width) / Exact(2),
        .Y = (Exact(1) - Exact::Float32(point[1]) / divisor) * Exact(height) / Exact(2)};
    result.FarDepth = std::min(result.FarDepth, z / divisor);
  }
  const int orientation = Edge(result.Corners[0], result.Corners[1], result.Corners[2]).Sign();
  result.SourceWindowOrientation = orientation;
  if (orientation < 0) { std::ranges::reverse(result.Corners); }
  for (size_t i = 0; i < result.Corners.size(); ++i) {
    if (Edge(result.Corners[i], result.Corners[(i + 1) % 4], result.Corners[(i + 2) % 4]).Sign() <=
        0) {
      return std::unexpected(Unknown::Nonconvex);
    }
  }
  return result;
}

namespace {
void Deduplicate(std::vector<ExactPoint> &points) {
  points.erase(std::ranges::unique(points).begin(), points.end());
  if (points.size() > 1 && points.front() == points.back()) { points.pop_back(); }
}

Exact Distance(const ExactPoint &point, size_t axis, const Exact &bound, bool upper) {
  const auto &coordinate = axis == 0 ? point.X : point.Y;
  return upper ? bound - coordinate : coordinate - bound;
}

std::vector<ExactPoint>
Clip(const std::vector<ExactPoint> &points, size_t axis, const Exact &bound, bool upper) {
  std::vector<ExactPoint> clipped;
  clipped.reserve(points.size() + 2);
  if (points.empty()) { return clipped; }
  auto previous = points.back();
  auto previousDistance = Distance(previous, axis, bound, upper);
  for (const auto &point : points) {
    const auto distance = Distance(point, axis, bound, upper);
    const bool in = distance.Sign() >= 0;
    const bool previousIn = previousDistance.Sign() >= 0;
    if (in != previousIn) {
      const auto part = previousDistance / (previousDistance - distance);
      clipped.push_back({.X = previous.X + (point.X - previous.X) * part,
                         .Y = previous.Y + (point.Y - previous.Y) * part});
    }
    if (in) { clipped.push_back(point); }
    previous = point;
    previousDistance = distance;
  }
  Deduplicate(clipped);
  return clipped;
}

using GridPoint = std::array<int32_t, 4>;

int64_t GridEdge(const GridPoint &a, const GridPoint &b, const GridPoint &point) {
  return int64_t{b[0] - a[0]} * (point[1] - a[1]) - int64_t{b[1] - a[1]} * (point[0] - a[0]);
}

bool Convex(const std::vector<GridPoint> &points) {
  if (points.size() < 3 || points.size() > MaximumCorners) { return false; }
  int64_t area = 0;
  for (size_t i = 0; i < points.size(); ++i) {
    const auto &a = points[i];
    const auto &b = points[(i + 1) % points.size()];
    area += int64_t{a[0]} * b[1] - int64_t{a[1]} * b[0];
    for (const auto &point : points) {
      if (GridEdge(a, b, point) < 0) { return false; }
    }
  }
  return area > 0;
}

std::optional<FixedPatch> Quantize(const Projected &source,
                                   const std::vector<ExactPoint> &points,
                                   uint32_t width,
                                   uint32_t height,
                                   uint32_t scale) {
  ExactPoint centre;
  for (const auto &point : points) {
    centre.X = centre.X + point.X;
    centre.Y = centre.Y + point.Y;
  }
  centre.X = centre.X / Exact(static_cast<double>(points.size()));
  centre.Y = centre.Y / Exact(static_cast<double>(points.size()));
  constexpr int MaximumShift = 20;
  for (int shift = MaximumShift; shift >= 0; --shift) {
    const Exact divisor(std::ldexp(1.0, shift));
    std::vector<GridPoint> grid;
    grid.reserve(points.size());
    for (const auto &point : points) {
      grid.push_back({((point.X * (divisor - Exact(1)) + centre.X) / divisor * Exact(scale))
                          .RoundSmallNonnegative(),
                      ((point.Y * (divisor - Exact(1)) + centre.Y) / divisor * Exact(scale))
                          .RoundSmallNonnegative(),
                      0,
                      0});
    }
    grid.erase(std::ranges::unique(grid).begin(), grid.end());
    if (grid.size() > 1 && grid.front() == grid.back()) { grid.pop_back(); }
    if (!Convex(grid)) { continue; }
    bool inside = true;
    for (const auto &point : grid) {
      inside = inside && point[0] >= 0 && point[1] >= 0 &&
               static_cast<uint32_t>(point[0]) <= width * scale &&
               static_cast<uint32_t>(point[1]) <= height * scale &&
               Inside(source.Corners,
                      {.X = Exact(point[0]) / Exact(scale), .Y = Exact(point[1]) / Exact(scale)});
    }
    if (!inside) { continue; }
    FixedPatch patch;
    std::ranges::copy(grid, patch.Corners.begin());
    patch.Meta = {static_cast<uint32_t>(grid.size()), width, height, scale};
    return patch;
  }
  return {};
}
}

std::expected<FixedPatch, Unknown>
Prepare(const SourceQuad &source, uint32_t width, uint32_t height) {
  auto projected = Project(source, width, height);
  if (!projected) { return std::unexpected(projected.error()); }
  std::vector<ExactPoint> points(projected->Corners.begin(), projected->Corners.end());
  points = Clip(points, 0, Exact(0), false);
  points = Clip(points, 0, Exact(width), true);
  points = Clip(points, 1, Exact(0), false);
  points = Clip(points, 1, Exact(height), true);
  if (points.size() < 3) { return std::unexpected(Unknown::Outside); }
  uint32_t scale = PreferredScale;
  while (std::max(width, height) * scale > MaximumCoordinate) { scale /= 2; }
  auto result = Quantize(*projected, points, width, height, scale);
  if (!result) { return std::unexpected(Unknown::Quantization); }
  auto depth = static_cast<float>(projected->FarDepth.Approximate());
  if (projected->FarDepth < Exact::Float32(depth)) { depth = std::nextafter(depth, 0.0f); }
  if (!std::isnormal(depth) || !(depth > 0) || projected->FarDepth < Exact::Float32(depth)) {
    return std::unexpected(Unknown::NoDepth);
  }
  result->Depth[0] = depth;
  return *result;
}

bool FullPixel(const FixedPatch &patch, uint32_t x, uint32_t y) {
  const auto count = patch.Meta[0];
  if (count < 3 || count > MaximumCorners || x >= patch.Meta[1] || y >= patch.Meta[2] ||
      patch.Meta[3] == 0) {
    return false;
  }
  const auto scale = static_cast<int32_t>(patch.Meta[3]);
  const GridPoint point{static_cast<int32_t>(x) * scale, static_cast<int32_t>(y) * scale, 0, 0};
  for (size_t i = 0; i < count; ++i) {
    const auto &a = patch.Corners[i];
    const auto &b = patch.Corners[(i + 1) % count];
    const int64_t least = GridEdge(a, b, point) +
                          std::min(int64_t{0}, int64_t{b[0] - a[0]} * scale) +
                          std::min(int64_t{0}, -int64_t{b[1] - a[1]} * scale);
    if (least < 0) { return false; }
  }
  return true;
}
}
