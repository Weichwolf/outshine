#include "WaterSurfacePreparation.h"
#include "WaterSurfaceMesh.h"

#include "TangentFrame.h"
#include "Earth.h"
#include "math/Vec3.h"

#include "PolygonTriangulation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <numeric>
#include <stop_token>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {

namespace {

constexpr double kAbsoluteAreaToleranceM2 = 1e-5;
constexpr double kRelativeAreaTolerance = 1e-6;

struct WaterVertex {
  double EastM;
  double NorthM;
  double UpM;
};

struct WaterPolygon {
  std::vector<std::vector<std::array<double, 2>>> Rings;
  std::vector<WaterVertex> Vertices;
  double AreaM2 = 0.0;
};

double RingAreaM2(std::span<const std::array<double, 2>> ring) {
  if (ring.size() < 3) { return 0.0; }
  double twice = 0.0;
  for (size_t at = 0; at < ring.size(); ++at) {
    const auto &a = ring[at];
    const auto &b = ring[(at + 1) % ring.size()];
    twice += a[0] * b[1] - b[0] * a[1];
  }
  return 0.5 * std::fabs(twice);
}

double TriangleAreaM2(const WaterVertex &a, const WaterVertex &b, const WaterVertex &c) {
  return 0.5 * ((b.EastM - a.EastM) * (c.NorthM - a.NorthM) -
                (b.NorthM - a.NorthM) * (c.EastM - a.EastM));
}

std::optional<WaterPolygon> ReadWaterPolygon(std::span<const WaterAsset::SurfaceRing> sourceRings,
                                             std::span<const double> geographicPoints,
                                             float levelM,
                                             const TangentFrame &frame) {
  if (sourceRings.empty()) { return std::nullopt; }
  WaterPolygon polygon;
  polygon.Rings.reserve(sourceRings.size());
  for (size_t ringIndex = 0; ringIndex < sourceRings.size(); ++ringIndex) {
    const auto &source = sourceRings[ringIndex];
    if (source.PointCount < 3 || source.FirstPoint > geographicPoints.size() / 2u ||
        source.PointCount > geographicPoints.size() / 2u - source.FirstPoint) {
      return std::nullopt;
    }
    auto &contour = polygon.Rings.emplace_back();
    contour.reserve(source.PointCount);
    for (uint32_t point = 0; point < source.PointCount; ++point) {
      const size_t at = (static_cast<size_t>(source.FirstPoint) + point) * 2u;
      const EastNorthUp placed = frame.ToLocalPosition({.LongitudeDeg = geographicPoints[at + 1],
                                                        .LatitudeDeg = geographicPoints[at],
                                                        .HeightM = static_cast<double>(levelM)});
      if (!std::isfinite(placed.EastM) || !std::isfinite(placed.NorthM) ||
          !std::isfinite(placed.UpM)) {
        return std::nullopt;
      }
      contour.push_back({placed.EastM, placed.NorthM});
      polygon.Vertices.push_back(
          {.EastM = placed.EastM, .NorthM = placed.NorthM, .UpM = placed.UpM});
    }
    const double areaM2 = RingAreaM2(contour);
    if (areaM2 <= 0.0) { return std::nullopt; }
    polygon.AreaM2 += ringIndex == 0 ? areaM2 : -areaM2;
  }
  if (polygon.AreaM2 <= 0.0) { return std::nullopt; }
  return polygon;
}

std::optional<std::vector<uint32_t>> TriangulateWaterPolygon(const WaterPolygon &polygon) {
  const auto triangulated = TriangulatePolygon(polygon.Rings);
  if (!triangulated) { return std::nullopt; }
  const auto &triangles = *triangulated;
  std::vector<uint32_t> oriented;
  oriented.reserve(triangles.size());
  double coveredAreaM2 = 0.0;
  for (size_t at = 0; at < triangles.size(); at += 3u) {
    const uint32_t a = triangles[at];
    uint32_t b = triangles[at + 1u];
    uint32_t c = triangles[at + 2u];
    if (a >= polygon.Vertices.size() || b >= polygon.Vertices.size() ||
        c >= polygon.Vertices.size()) {
      return std::nullopt;
    }
    const double areaM2 =
        TriangleAreaM2(polygon.Vertices[a], polygon.Vertices[b], polygon.Vertices[c]);
    if (!std::isfinite(areaM2) || areaM2 == 0.0) { return std::nullopt; }
    if (areaM2 < 0.0) { std::swap(b, c); }
    coveredAreaM2 += std::fabs(areaM2);
    oriented.insert(oriented.end(), {a, b, c});
  }
  if (std::fabs(coveredAreaM2 - polygon.AreaM2) >
      std::max(kAbsoluteAreaToleranceM2, polygon.AreaM2 * kRelativeAreaTolerance)) {
    return std::nullopt;
  }
  return oriented;
}

}

std::expected<WaterAsset, std::string> PrepareWaterSurfaces(const WaterAsset &water,
                                                            const TangentFrame &frame,
                                                            const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("water surface preparation canceled"); }
  if (water.SurfacesReady()) { return water; }
  std::vector<uint32_t> ordered(water.Surfaces().size());
  std::ranges::iota(ordered, uint32_t{0});
  std::ranges::sort(ordered, {}, [&water](uint32_t index) {
    return water.RingsOf(water.Surfaces()[index]).front().FirstPoint;
  });
  std::vector<Vec3> vertices;
  std::vector<WaterSurfaceMesh::Face> faces;
  WaterAsset::SurfaceGeometry prepared;
  constexpr size_t kMostVertices = std::numeric_limits<uint32_t>::max();
  constexpr size_t kMostFaces = std::numeric_limits<uint32_t>::max() / 2u;
  for (const uint32_t index : ordered) {
    if (stop.stop_requested()) { return std::unexpected("water surface preparation canceled"); }
    const auto &surface = water.Surfaces()[index];
    const auto polygon =
        ReadWaterPolygon(water.RingsOf(surface), water.Points(), surface.LevelM, frame);
    if (!polygon) {
      prepared.Rejections.push_back({.Surface = index});
      continue;
    }
    if (polygon->Vertices.size() > kMostVertices - vertices.size()) {
      prepared.Rejections.push_back(
          {.Surface = index, .Reason = WaterAsset::SurfaceRejectionReason::CapacityExceeded});
      continue;
    }
    const auto oriented = TriangulateWaterPolygon(*polygon);
    if (!oriented) {
      prepared.Rejections.push_back({.Surface = index});
      continue;
    }
    if (oriented->size() / 3u > kMostFaces - faces.size()) {
      prepared.Rejections.push_back(
          {.Surface = index, .Reason = WaterAsset::SurfaceRejectionReason::CapacityExceeded});
      continue;
    }
    const auto base = static_cast<uint32_t>(vertices.size());
    for (const auto &vertex : polygon->Vertices) {
      vertices.push_back({{vertex.EastM, vertex.NorthM, vertex.UpM}});
    }
    for (size_t at = 0; at < oriented->size(); at += 3u) {
      faces.push_back({.Vertices = {base + (*oriented)[at],
                                    base + (*oriented)[at + 1u],
                                    base + (*oriented)[at + 2u]},
                       .Surface = index});
    }
  }
  if (stop.stop_requested()) { return std::unexpected("water surface preparation canceled"); }
  auto mesh = WaterSurfaceMesh::Build(frame.Anchor(), std::move(vertices), std::move(faces));
  if (!mesh) { return std::unexpected("water surface mesh is invalid or exceeds its capacity"); }
  prepared.Mesh = std::move(*mesh);
  return water.WithSurfaces(std::move(prepared));
}
}
