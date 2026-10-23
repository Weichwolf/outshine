#include "RoofContourInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
namespace {
constexpr double kMinimumAreaToleranceM2 = 1e-7;
constexpr double kRelativeAreaTolerance = 1e-10;

int Winding(std::span<const Segment> segments, Point p) {
  int winding = 0;
  for (const auto &s : segments) {
    const auto a = s.From;
    const auto b = s.To;
    const double cross = (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]);
    if (a[1] <= p[1] && b[1] > p[1] && cross > 0) { ++winding; }
    if (a[1] > p[1] && b[1] <= p[1] && cross < 0) { --winding; }
  }
  return winding;
}

Checked<Geometry> PolygonizedFront(const Geos &g,
                                   std::span<const Loop> loops,
                                   std::span<const Point> coordinates,
                                   double tolerance) {
  std::vector<Geometry> lines;
  for (const auto &loop : loops) {
    const auto p = Subdivide(loop.Points, coordinates, tolerance);
    if (!p) { return std::unexpected(p.error()); }
    if (p->size() < 2) { continue; }
    for (size_t at = 0; at < p->size(); ++at) {
      const std::array<Point, 2> ends{(*p)[at], (*p)[(at + 1) % p->size()]};
      lines.push_back(Line(g, ends));
    }
  }
  const auto collection = Collection(g, std::move(lines));
  if (!collection) { return std::unexpected(Failure{"line collection"}); }
  const auto network = g.Own(GEOSUnaryUnion_r(g.Handle, collection.get()));
  if (!network) { return std::unexpected(Failure{"network union"}); }
  const int lineCount = GEOSGetNumGeometries_r(g.Handle, network.get());
  if (lineCount < 0) {
    return std::unexpected(Failure{HipRoofError::InvalidInput, "network shape"});
  }
  std::vector<const GEOSGeometry *> raw;
  raw.reserve(lineCount);
  for (int at = 0; at < lineCount; ++at) {
    raw.push_back(GEOSGetGeometryN_r(g.Handle, network.get(), at));
  }
  auto faces = g.Own(GEOSPolygonize_r(g.Handle, raw.data(), static_cast<unsigned int>(raw.size())));
  if (!faces) { return std::unexpected(Failure{"polygonize"}); }
  return faces;
}

Checked<std::vector<Geometry>> MaterialFaces(const Geos &g,
                                             const GEOSGeometry *faces,
                                             std::span<const Segment> segments,
                                             double tolerance) {
  std::vector<Geometry> keep;
  const int faceCount = GEOSGetNumGeometries_r(g.Handle, faces);
  if (faceCount < 0) {
    return std::unexpected(Failure{HipRoofError::InvalidInput, "polygonize shape"});
  }
  for (int at = 0; at < faceCount; ++at) {
    const auto *face = GEOSGetGeometryN_r(g.Handle, faces, at);
    double area = 0;
    double length = 0;
    if ((face == nullptr) || (GEOSArea_r(g.Handle, face, &area) == 0) ||
        (GEOSLength_r(g.Handle, face, &length) == 0)) {
      return std::unexpected(Failure{"face measure"});
    }
    if (area <= tolerance * length * 8) { continue; }
    const auto point = g.Own(GEOSPointOnSurface_r(g.Handle, face));
    auto p = Coordinates(g, point.get(), false);
    if (!p || p->size() != 1) { return std::unexpected(Failure{"face interior"}); }
    if (Winding(segments, p->front()) > 0) {
      keep.push_back(g.Own(GEOSGeom_clone_r(g.Handle, face)));
    }
  }
  return keep;
}

}

Checked<ContourBoundary> CollectBoundary(std::span<const Loop> loops, BoundaryLimits limits) {
  const auto ownerCount = limits.Owners;
  const auto tolerance = limits.ToleranceM;
  std::vector<Segment> segments;
  Ring vertices;
  double signedArea = 0;
  for (const auto &loop : loops) {
    if (loop.Points.empty() || loop.Owners.size() != loop.Points.size()) {
      return std::unexpected(Failure{HipRoofError::InvalidInput, "loop shape"});
    }
    for (size_t at = 0; at < loop.Points.size(); ++at) {
      const auto a = loop.Points[at];
      const auto b = loop.Points[(at + 1) % loop.Points.size()];
      if (loop.Owners[at] >= ownerCount) {
        return std::unexpected(Failure{HipRoofError::CapacityExceeded, "owner range"});
      }
      const auto delta = Subtract(a, b);
      if (std::hypot(delta[0], delta[1]) > tolerance) {
        segments.push_back({.From = a, .To = b, .Owner = loop.Owners[at]});
        vertices.push_back(a);
        vertices.push_back(b);
      }
      const auto u = Subtract(a, loop.Points[0]);
      const auto v = Subtract(b, loop.Points[0]);
      signedArea += (u[0] * v[1] - u[1] * v[0]) * .5;
    }
  }
  return ContourBoundary{
      .Segments = std::move(segments), .Coordinates = Unique(vertices), .SignedAreaM2 = signedArea};
}

Checked<ContourGeometry> PrepareContourGeometry(const Geos &g,
                                                std::span<const Loop> loops,
                                                std::span<const Segment> segments,
                                                std::span<const Point> coordinates,
                                                ContourMeasure measure) {
  const double tolerance = measure.ToleranceM;
  const double signedArea = measure.SignedAreaM2;
  std::vector<Geometry> pointParts;
  for (const auto p : coordinates) {
    pointParts.push_back(g.Own(GEOSGeom_createPointFromXY_r(g.Handle, p[0], p[1])));
  }
  auto anchors = Collection(g, std::move(pointParts), GEOS_MULTIPOINT);
  if (!anchors) { return std::unexpected(Failure{"anchor geometry"}); }
  auto faces = PolygonizedFront(g, loops, coordinates, tolerance);
  if (!faces) { return std::unexpected(faces.error()); }
  auto keep = MaterialFaces(g, faces->get(), segments, tolerance);
  if (!keep) { return std::unexpected(keep.error()); }
  const auto kept = Collection(g, std::move(*keep));
  if (!kept) { return std::unexpected(Failure{"kept geometry"}); }
  auto material = g.Own(GEOSUnaryUnion_r(g.Handle, kept.get()));
  if (!material || GEOSOrientPolygons_r(g.Handle, material.get(), 0) != 0) {
    return std::unexpected(Failure{"material union/orientation"});
  }
  double area = 0;
  if (GEOSArea_r(g.Handle, material.get(), &area) == 0) {
    return std::unexpected(Failure{"material area"});
  }
  if (std::abs(area - signedArea) >
      std::max(kMinimumAreaToleranceM2, std::abs(signedArea) * kRelativeAreaTolerance)) {
    return std::unexpected(Failure{"contour noding lost finite area"});
  }
  std::vector<Geometry> sources;
  for (const auto &s : segments) {
    const std::array<Point, 2> ends{s.From, s.To};
    sources.push_back(Line(g, ends));
    if (!sources.back()) { return std::unexpected(Failure{"source line"}); }
  }
  return ContourGeometry{.Material = std::move(material),
                         .Anchors = std::move(anchors),
                         .Sources = std::move(sources)};
}
}
