#include "RoofContourInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
Point Add(Point a, Point b) {
  return {a[0] + b[0], a[1] + b[1]};
}

Point Subtract(Point a, Point b) {
  return {a[0] - b[0], a[1] - b[1]};
}

Point Scale(Point p, double factor) {
  return {p[0] * factor, p[1] * factor};
}

double Dot(Point a, Point b) {
  return a[0] * b[0] + a[1] * b[1];
}

namespace {
constexpr double kMaximumInterfaceCorrectionM = 5e-5;
constexpr double kMinimumPlaneDifferenceSquared = 1e-24;
using Rotation = std::array<Vector3, 3>;

struct ColumnPair {
  size_t First, Second;
};

bool RotateColumns(std::span<Vector3> rows, Rotation &rotation, ColumnPair pair) {
  const auto p = pair.First;
  const auto q = pair.Second;
  double aa = 0;
  double bb = 0;
  double ab = 0;
  for (const auto &row : rows) {
    aa += row[p] * row[p];
    bb += row[q] * row[q];
    ab += row[p] * row[q];
  }
  if (std::abs(ab) <= std::numeric_limits<double>::epsilon() * std::sqrt(aa) * std::sqrt(bb)) {
    return false;
  }
  const double tau = (bb - aa) / (2 * ab);
  const double tangent = std::copysign(1.0, tau) / (std::abs(tau) + std::hypot(1.0, tau));
  const double cosine = 1 / std::sqrt(1 + tangent * tangent);
  const double sine = tangent * cosine;
  for (auto &row : rows) {
    const auto a = row[p];
    const auto b = row[q];
    row[p] = cosine * a - sine * b;
    row[q] = sine * a + cosine * b;
  }
  for (auto &row : rotation) {
    const auto a = row[p];
    const auto b = row[q];
    row[p] = cosine * a - sine * b;
    row[q] = sine * a + cosine * b;
  }
  return true;
}

void Orthogonalize(std::span<Vector3> rows, Rotation &rotation) {
  constexpr size_t kMaximumSweeps = 64;
  for (size_t sweep = 0; sweep < kMaximumSweeps; ++sweep) {
    bool changed = false;
    for (size_t p = 0; p < 3; ++p) {
      for (size_t q = p + 1; q < 3; ++q) {
        changed = RotateColumns(rows, rotation, {.First = p, .Second = q}) || changed;
      }
    }
    if (!changed) { break; }
  }
}

struct InterfacePlanes {
  std::span<const Point> Normals;
  std::span<const double> Intercepts;
};

struct InterfacePoint {
  Point Position;
  double HeightM;
};

std::optional<InterfacePoint>
TwoPlanePoint(Point center, InterfacePlanes planes, std::span<const size_t> ids) {
  Point axis{};
  size_t selected = ids.front();
  double best = 0;
  for (const auto id : ids) {
    const auto difference = Subtract(planes.Normals[id], planes.Normals[ids.front()]);
    const double norm = Dot(difference, difference);
    if (norm > best) {
      best = norm;
      axis = difference;
      selected = id;
    }
  }
  if (best <= kMinimumPlaneDifferenceSquared) { return std::nullopt; }
  const auto candidate = Subtract(
      center,
      Scale(axis,
            (Dot(axis, center) + planes.Intercepts[selected] - planes.Intercepts[ids.front()]) /
                best));
  return InterfacePoint{.Position = candidate,
                        .HeightM = Dot(planes.Normals[ids.front()], candidate) +
                                   planes.Intercepts[ids.front()]};
}

std::optional<InterfacePoint> InterfaceTarget(std::span<const Point> points,
                                              std::span<const size_t> indices,
                                              InterfacePlanes planes,
                                              std::span<const size_t> ids,
                                              double tolerance) {
  Point center{};
  for (const auto at : indices) { center = Add(center, points[at]); }
  center = Scale(center, 1 / static_cast<double>(indices.size()));
  double height = 0;
  for (const auto id : ids) { height += Dot(planes.Normals[id], center) + planes.Intercepts[id]; }
  height /= static_cast<double>(ids.size());
  std::vector<Vector3> matrix;
  std::vector<double> residual;
  double maximumResidual = 0;
  for (const auto id : ids) {
    matrix.push_back({planes.Normals[id][0], planes.Normals[id][1], -1});
    const double value = -planes.Intercepts[id] - (planes.Normals[id][0] * center[0] +
                                                   planes.Normals[id][1] * center[1] - height);
    residual.push_back(value);
    maximumResidual = std::max(maximumResidual, std::abs(value));
  }
  const auto solved = SolveSmall(matrix, residual);
  Vector3 solution{
      center[0] + solved.Solution[0], center[1] + solved.Solution[1], height + solved.Solution[2]};
  if (maximumResidual <= tolerance) { solution = {center[0], center[1], height}; }
  if (solved.Rank == 3) {
    return InterfacePoint{.Position = {solution[0], solution[1]}, .HeightM = solution[2]};
  }
  if (solved.Rank == 2) { return TwoPlanePoint(center, planes, ids); }
  return std::nullopt;
}

std::optional<double> InterfaceCorrection(std::span<const Point> points,
                                          std::span<const size_t> indices,
                                          InterfacePlanes planes,
                                          std::span<const size_t> ids,
                                          InterfacePoint target,
                                          double tolerance) {
  double correction = 0;
  double residual = 0;
  for (const auto at : indices) {
    const auto delta = Subtract(points[at], target.Position);
    correction = std::max(correction, std::hypot(delta[0], delta[1]));
  }
  for (const auto id : ids) {
    residual = std::max(residual,
                        std::abs(Dot(planes.Normals[id], target.Position) + planes.Intercepts[id] -
                                 target.HeightM));
  }
  if (correction <= kMaximumInterfaceCorrectionM && residual <= tolerance) { return correction; }
  return std::nullopt;
}
}

LeastSquares SolveSmall(std::vector<Vector3> rows, std::span<const double> rhs) {
  Rotation rotation{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
  Orthogonalize(rows, rotation);
  Vector3 norm{};
  for (const auto &row : rows) {
    for (size_t k = 0; k < 3; ++k) { norm[k] += row[k] * row[k]; }
  }
  const double maximum = std::sqrt(*std::ranges::max_element(norm));
  const double cutoff = maximum * std::numeric_limits<double>::epsilon() *
                        static_cast<double>(std::max(rows.size(), size_t{3}));
  LeastSquares result;
  for (size_t k = 0; k < 3; ++k) {
    if (std::sqrt(norm[k]) <= cutoff) { continue; }
    ++result.Rank;
    double coefficient = 0;
    for (size_t at = 0; at < rows.size(); ++at) { coefficient += rows[at][k] * rhs[at]; }
    coefficient /= norm[k];
    for (size_t axis = 0; axis < 3; ++axis) {
      result.Solution[axis] += rotation[axis][k] * coefficient;
    }
  }
  return result;
}

Checked<Refined> Refine(std::vector<Point> points,
                        std::span<const int> owners,
                        std::span<const Point> normals,
                        std::span<const double> intercepts,
                        RefinementLimits limits) {
  const auto fixed = limits.FixedVertices;
  const auto tolerance = limits.PlaneToleranceM / 2;
  const auto clustered = WeldVertices(points, kMaximumInterfaceCorrectionM);
  if (!clustered) { return std::unexpected(clustered.error()); }
  std::map<Point, std::vector<size_t>> groups;
  for (size_t at = 0; at < points.size(); ++at) { groups[(*clustered)[at]].push_back(at); }
  double largest = 0;
  const InterfacePlanes planes{.Normals = normals, .Intercepts = intercepts};
  for (const auto &[key, indices] : groups) {
    (void)key;
    if (indices.size() < 2 || indices.front() < fixed) { continue; }
    std::set<size_t> incident;
    for (const auto at : indices) {
      if (owners[at] < 0) { return std::unexpected(Failure{"missing incident owner"}); }
      incident.insert(static_cast<size_t>(owners[at]));
    }
    if (incident.size() < 2) { continue; }
    const std::vector<size_t> ids(incident.begin(), incident.end());
    const auto candidate = InterfaceTarget(points, indices, planes, ids, tolerance);
    if (!candidate) { continue; }
    const auto correction =
        InterfaceCorrection(points, indices, planes, ids, *candidate, tolerance);
    if (!correction) { continue; }
    for (const auto at : indices) { points[at] = candidate->Position; }
    largest = std::max(largest, *correction);
  }
  return Refined{.Points = std::move(points), .MaximumM = largest};
}
}
