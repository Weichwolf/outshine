#include "GroundPatchMesh.h"
#include "GeosTriangle.h"
#include <geos_c.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr uint32_t kCells = 8, kSide = kCells + 1;
using Point = std::array<double, 2>;

Point Position(uint32_t node) {
  return {static_cast<double>(node % kSide) / kCells, static_cast<double>(node / kSide) / kCells};
}

std::array<double, 3> Weights(const GroundPatchFace &face, Point point) {
  const auto a = Position(face[0]), b = Position(face[1]), c = Position(face[2]);
  const double dx = point[0] - a[0], dy = point[1] - a[1];
  const double bx = b[0] - a[0], by = b[1] - a[1], cx = c[0] - a[0], cy = c[1] - a[1];
  const double determinant = bx * cy - by * cx;
  const double v = (dx * cy - dy * cx) / determinant;
  const double w = (bx * dy - by * dx) / determinant;
  return {1 - v - w, v, w};
}

Spatial::GeosGeometry Polygon(GEOSContextHandle_t context, const GroundPatchFace &face) {
  std::array<Vec3, 3> corners;
  for (size_t i = 0; i < corners.size(); ++i) {
    const auto p = Position(face[i]);
    corners[i] = {{p[0], p[1], 0}};
  }
  return Spatial::ProjectedTriangle(context, corners);
}

std::expected<void, GroundPatchMeshError> Record(GEOSContextHandle_t context,
                                                 const GEOSGeometry &overlap,
                                                 const GroundPatchFace &fine,
                                                 const GroundPatchFace &coarse,
                                                 std::map<Point, GroundPatchSample> &samples) {
  double area = 0;
  if (GEOSArea_r(context, &overlap, &area) == 0) {
    return std::unexpected(GroundPatchMeshError::Backend);
  }
  if (area == 0) { return {}; }
  const auto *ring = GEOSGetExteriorRing_r(context, &overlap);
  const auto *coordinates = ring != nullptr ? GEOSGeom_getCoordSeq_r(context, ring) : nullptr;
  unsigned count = 0;
  if (coordinates == nullptr || GEOSCoordSeq_getSize_r(context, coordinates, &count) == 0) {
    return std::unexpected(GroundPatchMeshError::Backend);
  }
  for (unsigned i = 0; i < count; ++i) {
    Point point{};
    if (GEOSCoordSeq_getXY_r(context, coordinates, i, &point[0], &point[1]) == 0) {
      return std::unexpected(GroundPatchMeshError::Backend);
    }
    if (std::ranges::any_of(coarse, [&](uint32_t node) { return Position(node) == point; })) {
      continue;
    }
    const auto f = Weights(fine, point), c = Weights(coarse, point);
    GroundPatchSample sample{};
    for (size_t k = 0; k < f.size(); ++k) {
      if (!std::isfinite(f[k]) || !std::isfinite(c[k]) || f[k] < -1e-12 || c[k] < -1e-12) {
        return std::unexpected(GroundPatchMeshError::InvalidRefinement);
      }
      sample.Nodes[k] = fine[k];
      sample.Nodes[k + 3] = coarse[k];
      sample.Weights[k] = f[k];
      sample.Weights[k + 3] = -c[k];
    }
    samples.try_emplace(point, sample);
  }
  return {};
}
}

std::vector<GroundPatchFace> GroundPatchFaces(uint32_t stride) {
  std::vector<GroundPatchFace> result;
  if (stride == 0 || stride > kCells || (stride & (stride - 1u)) != 0) { return result; }
  const auto at = [](uint32_t x, uint32_t y) { return y * kSide + x; };
  for (uint32_t y = 0; y < kCells; y += stride) {
    for (uint32_t x = 0; x < kCells; x += stride) {
      if (stride == 1 || (x > 0 && y > 0 && x + stride < kCells && y + stride < kCells)) {
        result.push_back({at(x, y), at(x, y + stride), at(x + stride, y + stride)});
        result.push_back({at(x, y), at(x + stride, y + stride), at(x + stride, y)});
        continue;
      }
      std::array<uint32_t, 4u * kCells> boundary{};
      size_t count = 0;
      const auto edge = [&](uint32_t fromX, uint32_t fromY, int dx, int dy, bool fine) {
        const uint32_t step = fine ? 1u : stride;
        for (uint32_t k = 0; k < stride; k += step) {
          boundary[count++] =
              at(static_cast<uint32_t>(static_cast<int>(fromX) + dx * static_cast<int>(k)),
                 static_cast<uint32_t>(static_cast<int>(fromY) + dy * static_cast<int>(k)));
        }
      };
      edge(x, y, 1, 0, y == 0);
      edge(x + stride, y, 0, 1, x + stride == kCells);
      edge(x + stride, y + stride, -1, 0, y + stride == kCells);
      edge(x, y + stride, 0, -1, x == 0);
      const uint32_t centre = at(x + stride / 2, y + stride / 2);
      for (size_t k = 0; k < count; ++k) {
        result.push_back({centre, boundary[(k + 1) % count], boundary[k]});
      }
    }
  }
  return result;
}

std::expected<GroundPatchMesh, GroundPatchMeshError> MakeGroundPatchMesh(uint32_t stride) {
  GroundPatchMesh result{.Faces = GroundPatchFaces(stride), .Samples = {}};
  if (result.Faces.empty()) { return std::unexpected(GroundPatchMeshError::InvalidStride); }
  if (stride == 1) { return result; }
  using Context =
      std::unique_ptr<std::remove_pointer_t<GEOSContextHandle_t>, decltype(&GEOS_finish_r)>;
  Context context(GEOS_init_r(), &GEOS_finish_r);
  if (!context) { return std::unexpected(GroundPatchMeshError::Backend); }
  std::map<Point, GroundPatchSample> samples;
  const auto fine = GroundPatchFaces(1);
  std::vector<Spatial::GeosGeometry> polygons;
  polygons.reserve(fine.size());
  for (const auto &face : fine) {
    auto polygon = Polygon(context.get(), face);
    if (!polygon) { return std::unexpected(GroundPatchMeshError::Backend); }
    polygons.push_back(std::move(polygon));
  }
  for (const auto &coarse : result.Faces) {
    auto polygon = Polygon(context.get(), coarse);
    if (!polygon) { return std::unexpected(GroundPatchMeshError::Backend); }
    for (size_t i = 0; i < fine.size(); ++i) {
      const char intersects = GEOSIntersects_r(context.get(), polygon.get(), polygons[i].get());
      if (intersects == 2) { return std::unexpected(GroundPatchMeshError::Backend); }
      if (intersects == 0) { continue; }
      const Spatial::GeosGeometry overlap(
          GEOSIntersection_r(context.get(), polygon.get(), polygons[i].get()), {context.get()});
      if (!overlap) { return std::unexpected(GroundPatchMeshError::Backend); }
      if (const auto recorded = Record(context.get(), *overlap, fine[i], coarse, samples);
          !recorded) {
        return std::unexpected(recorded.error());
      }
    }
  }
  result.Samples.reserve(samples.size());
  for (const auto &[point, sample] : samples) { result.Samples.push_back(sample); }
  return result;
}

double GroundPatchHeightError(std::span<const float, 81> heights,
                              std::span<const GroundPatchSample> samples) noexcept {
  double result = 0, scale = 0;
  for (const float h : heights) {
    if (!std::isfinite(h)) { return std::numeric_limits<double>::infinity(); }
    scale = std::max(scale, std::abs(static_cast<double>(h)));
  }
  for (const auto &sample : samples) {
    double difference = 0;
    for (size_t i = 0; i < sample.Nodes.size(); ++i) {
      difference += static_cast<double>(heights[sample.Nodes[i]]) * sample.Weights[i];
    }
    result = std::max(result, std::abs(difference));
  }
  return result + 64 * std::numeric_limits<double>::epsilon() * (scale + 1);
}
}
