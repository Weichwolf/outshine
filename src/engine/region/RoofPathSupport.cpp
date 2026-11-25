#include "RoofPathSupport.h"
#include "PreparedBuildingAssets.h"
#include "PublishedRegion.h"
#include "TangentFrame.h"
#include "spatial/GeosPolygon.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <geos_c.h>

namespace outshine {
namespace {
struct Geos {
  GEOSContextHandle_t Handle = GEOS_init_r();

  ~Geos() {
    if (Handle != nullptr) { GEOS_finish_r(Handle); }
  }
};

struct Path {
  size_t Way;
  int32_t Floor;
  Box Bounds;
  Spatial::GeosGeometry Ribbon;
};

struct Roof {
  Spatial::GeosGeometry Shape;
  EastNorthUp Point, Normal;
  Box Bounds;
  int32_t Floor = 0;
};

struct SurfaceCoverage {
  std::vector<size_t> Roofs;
  Spatial::GeosGeometry Shape;
};

struct PathRoofs {
  SurfaceCoverage Roof;
  SurfaceCoverage Interior;
};

Spatial::GeosGeometry Ribbon(GEOSContextHandle_t ctx, const PolygonRing &axis, double half);

struct Support {
  Geos Context;
  std::vector<Roof> Roofs;
  std::unordered_map<size_t, PathRoofs> Ways;

  [[nodiscard]] std::optional<double> UpM(const SurfaceCoverage &bound,
                                          const RoadStation &at) const {
    auto *const ctx = Context.Handle;
    const Spatial::GeosGeometry point(GEOSGeom_createPointFromXY_r(ctx, at.EastM, at.NorthM),
                                      {ctx});
    if (!point) { return std::nullopt; }
    std::optional<double> height;
    for (const size_t index : bound.Roofs) {
      const Roof &roof = Roofs[index];
      if (GEOSCovers_r(ctx, roof.Shape.get(), point.get()) != 1) { continue; }
      const double up = roof.Point.UpM - (roof.Normal.EastM * (at.EastM - roof.Point.EastM) +
                                          roof.Normal.NorthM * (at.NorthM - roof.Point.NorthM)) /
                                             roof.Normal.UpM;
      if (!height || up > *height) { height = up; }
    }
    return height;
  }

  [[nodiscard]] RoadDeckSupport
  Profile(size_t way, std::span<RoadStation> stations, double halfWidthM) const {
    const auto found = Ways.find(way);
    if (found == Ways.end() || stations.empty()) { return RoadDeckSupport::Terrain; }
    const auto &bound = found->second;
    auto *const ctx = Context.Handle;
    Spatial::GeosGeometry section(nullptr, {ctx});
    if (stations.size() > 1) {
      PolygonRing axis;
      axis.reserve(stations.size());
      for (const auto &at : stations) { axis.push_back({at.EastM, at.NorthM}); }
      section = Ribbon(ctx, axis, halfWidthM);
    } else {
      section.reset(
          GEOSGeom_createPointFromXY_r(ctx, stations.front().EastM, stations.front().NorthM));
    }
    if (!section) { return RoadDeckSupport::Terrain; }
    if (!bound.Roof.Shape || GEOSCovers_r(ctx, bound.Roof.Shape.get(), section.get()) != 1) {
      return bound.Interior.Shape &&
                     GEOSCovers_r(ctx, bound.Interior.Shape.get(), section.get()) == 1
                 ? RoadDeckSupport::Interior
                 : RoadDeckSupport::Terrain;
    }
    std::vector<double> grades;
    grades.reserve(stations.size());
    for (const auto &at : stations) {
      const auto up = UpM(bound.Roof, at);
      if (!up) { return RoadDeckSupport::Terrain; }
      grades.push_back(*up);
    }
    for (size_t at = 0; at < stations.size(); ++at) { stations[at].GradeM = grades[at]; }
    return RoadDeckSupport::Roof;
  }
};

bool IntersectsXY(const Box &a, const Box &b) {
  return !a.Empty() && !b.Empty() && a.Min[0] <= b.Max[0] && a.Max[0] >= b.Min[0] &&
         a.Min[1] <= b.Max[1] && a.Max[1] >= b.Min[1];
}

Spatial::GeosGeometry Ribbon(GEOSContextHandle_t ctx, const PolygonRing &axis, double half) {
  auto *const seq = GEOSCoordSeq_create_r(ctx, static_cast<unsigned>(axis.size()), 2);
  if (seq == nullptr) { return {nullptr, {ctx}}; }
  for (size_t at = 0; at < axis.size(); ++at) {
    if (GEOSCoordSeq_setXY_r(ctx, seq, static_cast<unsigned>(at), axis[at][0], axis[at][1]) != 1) {
      GEOSCoordSeq_destroy_r(ctx, seq);
      return {nullptr, {ctx}};
    }
  }
  const Spatial::GeosGeometry line(GEOSGeom_createLineString_r(ctx, seq), {ctx});
  if (!line) { return {nullptr, {ctx}}; }
  return {GEOSBufferWithStyle_r(ctx, line.get(), half, 1, GEOSBUF_CAP_FLAT, GEOSBUF_JOIN_ROUND, 1),
          {ctx}};
}

std::vector<Path> CandidatePaths(const Generators::Osm::RegionSources &sources,
                                 const TangentFrame &frame,
                                 GEOSContextHandle_t ctx) {
  const auto &vectors = *sources.Vectors;
  const auto points = vectors.Points();
  std::vector<Path> paths;
  for (size_t index = 0; index < sources.Ways.Ways().size(); ++index) {
    const auto &way = sources.Ways.Ways()[index];
    const auto floor = way.Floor.value_or(0);
    if (floor <= 0 || way.PointCount < 2 || way.HalfWidthM <= 0) { continue; }
    const auto source = sources.Ways.SourceOf(vectors, index);
    if (!source) { continue; }
    const auto &feature = vectors.Features()[source->Feature];
    const auto indoor = vectors.Str(feature, "indoor");
    if (vectors.Num(feature, "indoor", 0) != 0 || indoor == "yes" || indoor == "true" ||
        vectors.Str(feature, "kind") == "steps" || vectors.Str(feature, "subclass") == "steps") {
      continue;
    }
    PolygonRing axis;
    axis.reserve(way.PointCount);
    Box bounds;
    for (size_t at = way.FirstPoint; at < size_t{way.FirstPoint} + way.PointCount; ++at) {
      const auto local = frame.ToLocalGroundPosition(
          {.LongitudeDeg = points[at * 2 + 1], .LatitudeDeg = points[at * 2]});
      axis.push_back({local.EastM, local.NorthM});
      bounds.Cover(Vec3{{local.EastM - way.HalfWidthM, local.NorthM - way.HalfWidthM, 0}});
      bounds.Cover(Vec3{{local.EastM + way.HalfWidthM, local.NorthM + way.HalfWidthM, 0}});
    }
    auto ribbon = Ribbon(ctx, axis, way.HalfWidthM);
    if (ribbon) {
      paths.push_back(
          {.Way = index, .Floor = floor, .Bounds = bounds, .Ribbon = std::move(ribbon)});
    }
  }
  return paths;
}

EastNorthUp RoofVertex(const Generators::BuildingSurface &surface,
                       const Vec3 &origin,
                       const TangentFrame &frame,
                       EastNorth at,
                       double height) {
  const auto &axes = surface.Axes();
  return frame.ToLocalPosition(origin + axes.East * at.EastM + axes.North * at.NorthM +
                               axes.Up * height);
}

Spatial::GeosGeometry RoofPolygon(GEOSContextHandle_t ctx,
                                  const Generators::BuildingSurface &surface,
                                  const Vec3 &origin,
                                  const TangentFrame &frame,
                                  const Generators::BuildingShape &part) {
  std::vector<PolygonRing> loops;
  const auto add = [&](std::span<const EastNorth> ring) {
    auto &loop = loops.emplace_back();
    loop.reserve(ring.size());
    for (const EastNorth p : ring) {
      const auto at = RoofVertex(surface, origin, frame, p, part.TopM());
      loop.push_back({at.EastM, at.NorthM});
    }
  };
  add(part.Ring);
  for (const auto &hole : part.Holes) { add(hole); }
  return Spatial::MakeGeosPolygon(ctx, loops);
}

void CollectRoofs(Support &support,
                  const Generators::PreparedStructureTile &base,
                  const TangentFrame &frame,
                  std::span<const Path> paths,
                  const std::atomic_bool &stopping) {
  auto *const ctx = support.Context.Handle;
  for (const auto &surface : base.Surfaces) {
    if (stopping.load(std::memory_order_relaxed)) { return; }
    if (surface.Bounds().Empty()) { continue; }
    Box bounds;
    for (unsigned corner = 0; corner < 8; ++corner) {
      const auto at = frame.ToLocalPosition(base.AnchorEcef + surface.Bounds().Corner(corner));
      bounds.Cover(Vec3{{at.EastM, at.NorthM, at.UpM}});
    }
    if (std::ranges::none_of(paths,
                             [&](const Path &path) { return IntersectsXY(path.Bounds, bounds); }) ||
        !surface.RequireShapes()) {
      continue;
    }
    const Vec3 origin = base.AnchorEcef + surface.Origin();
    const auto normal = frame.ToLocalDirection(surface.Axes().Up);
    if (normal.UpM <= 0) { continue; }
    for (const auto &part : surface.Shapes()) {
      if (part.Roof != Generators::RoofKind::Flat || part.RoundFootprint || part.Ring.empty()) {
        continue;
      }
      auto polygon = RoofPolygon(ctx, surface, origin, frame, part);
      if (!polygon || GEOSisValid_r(ctx, polygon.get()) != 1) { continue; }
      support.Roofs.push_back(
          {.Shape = std::move(polygon),
           .Point = RoofVertex(surface, origin, frame, part.Ring.front(), part.TopM()),
           .Normal = normal,
           .Bounds = bounds});
    }
  }
}

bool AddRoof(SurfaceCoverage &coverage, GEOSContextHandle_t ctx, const Roof &roof, size_t index) {
  coverage.Shape.reset(coverage.Shape ? GEOSUnion_r(ctx, coverage.Shape.get(), roof.Shape.get())
                                      : GEOSGeom_clone_r(ctx, roof.Shape.get()));
  if (!coverage.Shape) { return false; }
  coverage.Roofs.push_back(index);
  return true;
}

void BindPaths(Support &support, std::span<const Path> paths) {
  auto *const ctx = support.Context.Handle;
  std::vector<std::vector<size_t>> matches(paths.size());
  for (size_t at = 0; at < paths.size(); ++at) {
    const Path &path = paths[at];
    for (size_t index = 0; index < support.Roofs.size(); ++index) {
      auto &roof = support.Roofs[index];
      if (!IntersectsXY(path.Bounds, roof.Bounds) ||
          GEOSIntersects_r(ctx, roof.Shape.get(), path.Ribbon.get()) != 1) {
        continue;
      }
      matches[at].push_back(index);
      roof.Floor = std::max(roof.Floor, path.Floor);
    }
  }
  for (size_t at = 0; at < paths.size(); ++at) {
    PathRoofs bound{.Roof = {.Roofs = {}, .Shape = {nullptr, {ctx}}},
                    .Interior = {.Roofs = {}, .Shape = {nullptr, {ctx}}}};
    for (const size_t index : matches[at]) {
      const auto &roof = support.Roofs[index];
      auto &cover = roof.Floor == paths[at].Floor ? bound.Roof : bound.Interior;
      if (!AddRoof(cover, ctx, roof, index)) {
        bound.Roof.Shape.reset();
        bound.Interior.Shape.reset();
        break;
      }
    }
    if (bound.Roof.Shape || bound.Interior.Shape) {
      support.Ways.emplace(paths[at].Way, std::move(bound));
    }
  }
}
}

RoadDeckProfile PrepareRoofDeckProfile(const Generators::Osm::RegionSources &sources,
                                       Generators::PreparedBuildingAssets &assets,
                                       std::span<const std::string> baseKeys,
                                       const TangentFrame &frame,
                                       const std::atomic_bool &stopping) {
  if (!sources.Vectors || baseKeys.empty()) { return {}; }
  auto support = std::make_shared<Support>();
  if (support->Context.Handle == nullptr) { return {}; }
  const auto paths = CandidatePaths(sources, frame, support->Context.Handle);
  if (paths.empty()) { return {}; }
  for (const auto &key : baseKeys) {
    if (stopping.load(std::memory_order_relaxed)) { return {}; }
    const auto base = assets.Load(key);
    if (base && *base) { CollectRoofs(*support, **base, frame, paths, stopping); }
  }
  if (support->Roofs.empty()) { return {}; }
  BindPaths(*support, paths);
  if (support->Ways.empty()) { return {}; }
  return [support =
              std::move(support)](size_t way, std::span<RoadStation> stations, double halfWidthM) {
    return support->Profile(way, stations, halfWidthM);
  };
}
}
