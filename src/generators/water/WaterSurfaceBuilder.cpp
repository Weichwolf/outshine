#include "WaterSurfaceBuilder.h"

#include "TangentFrame.h"
#include "math/RenderFrame.h"

#include <mapbox/earcut.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
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

std::optional<WaterPolygon>
ReadWaterPolygon(std::span<const outshine::Ground::WaterField::SurfaceRing> sourceRings,
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
  const std::vector<uint32_t> triangles = mapbox::earcut<uint32_t>(polygon.Rings);
  if (triangles.empty() || triangles.size() % 3u != 0) { return std::nullopt; }
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

std::expected<WaterSurfaceMetrics, std::string>
AppendWaterSurfaceGeometry(Geometry &geometry,
                           MaterialInstance material,
                           const outshine::Ground::WaterField &water,
                           std::span<const double> geographicPoints,
                           const TangentFrame &frame) {
  WaterSurfaceMetrics metrics;
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> texcoords;
  std::vector<uint32_t> indices;
  std::vector<const Ground::WaterField::Surface *> orderedSurfaces;
  orderedSurfaces.reserve(water.Surfaces().size());
  for (const auto &surface : water.Surfaces()) { orderedSurfaces.push_back(&surface); }
  std::ranges::sort(orderedSurfaces, {}, [&water](const auto *surface) {
    return water.RingsOf(*surface).front().FirstPoint;
  });
  for (const auto *surface : orderedSurfaces) {
    const auto polygon =
        ReadWaterPolygon(water.RingsOf(*surface), geographicPoints, surface->LevelM, frame);
    if (!polygon ||
        polygon->Vertices.size() > std::numeric_limits<uint32_t>::max() - positions.size() / 3u) {
      ++metrics.RefusedTopology;
      continue;
    }
    const auto oriented = TriangulateWaterPolygon(*polygon);
    if (!oriented) {
      ++metrics.RefusedTopology;
      continue;
    }
    const auto base = static_cast<uint32_t>(positions.size() / 3u);
    for (const WaterVertex &vertex : polygon->Vertices) {
      positions.push_back(static_cast<float>(vertex.EastM));
      positions.push_back(static_cast<float>(vertex.UpM));
      positions.push_back(static_cast<float>(RenderFrame::ZOfNorth(vertex.NorthM)));
      normals.insert(normals.end(), {0.0f, 1.0f, 0.0f});
      texcoords.push_back(static_cast<float>(vertex.EastM));
      texcoords.push_back(static_cast<float>(vertex.NorthM));
    }
    for (const uint32_t index : *oriented) { indices.push_back(base + index); }
    metrics.Triangles += oriented->size() / 3u;
    ++metrics.Laid;
  }
  if (indices.empty()) { return metrics; }
  const auto part = geometry.addPart("water", material);
  if (!part || !geometry.setPositions(*part, positions) || !geometry.setNormals(*part, normals) ||
      !geometry.setTriangles(*part, indices) || !geometry.setTexture(*part, texcoords, 0)) {
    return std::unexpected("could not append native water surface geometry");
  }
  return metrics;
}

}
