#include "WaterSurfacePreparation.h"
#include "WaterSurfaceMesh.h"
#include "RiverBodyHeight.h"

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

enum class SurfaceState { Pending, Unprofiled, Prepared, Rejected };

struct SurfaceAssembly {
  std::vector<Vec3> Vertices;
  std::vector<WaterSurfaceMesh::Face> Faces;
  WaterAsset::SurfaceGeometry Product;
  std::vector<SurfaceState> States;

  explicit SurfaceAssembly(size_t count) : States(count, SurfaceState::Pending) {
    Product.FaceSourceOffsets.push_back(0);
  }

  void Reject(uint32_t source, WaterAsset::SurfaceRejectionReason reason) {
    Product.Rejections.push_back({.Surface = source, .Reason = reason});
    States[source] = SurfaceState::Rejected;
  }

  void DeclineProfile(uint32_t source, WaterAsset::SurfaceRejectionReason reason) {
    if (States[source] == SurfaceState::Unprofiled) { return; }
    Product.ProfileDeclines.push_back({.Surface = source, .Reason = reason});
    States[source] = SurfaceState::Unprofiled;
  }

  void Source(std::span<const uint32_t> sources) {
    Product.FaceSources.insert(Product.FaceSources.end(), sources.begin(), sources.end());
    Product.FaceSourceOffsets.push_back(static_cast<uint32_t>(Product.FaceSources.size()));
  }
};

std::vector<uint32_t> Sources(const RiverBody &body) {
  auto sources = body.Surface.FaceSources;
  std::ranges::sort(sources);
  const auto duplicate = std::ranges::unique(sources);
  sources.erase(duplicate.begin(), duplicate.end());
  return sources;
}

std::expected<void, std::string> AppendRiver(const WaterAsset &water,
                                             const TangentFrame &frame,
                                             const RiverBody &body,
                                             const std::stop_token &stop,
                                             SurfaceAssembly &assembly) {
  const auto sources = Sources(body);
  auto fit = FitRiverBody(water, frame, body, {}, stop);
  if (!fit) {
    if (fit.error() == WaterHeightError::Canceled) {
      return std::unexpected("river surface preparation canceled");
    }
    const auto reason = fit.error() == WaterHeightError::CapacityExceeded
                            ? WaterAsset::SurfaceRejectionReason::CapacityExceeded
                            : WaterAsset::SurfaceRejectionReason::InvalidHeightModel;
    for (const auto source : sources) { assembly.DeclineProfile(source, reason); }
    return {};
  }
  if (fit->VerticesEnuM.size() > std::numeric_limits<uint32_t>::max() - assembly.Vertices.size() ||
      body.Surface.Faces.size() >
          std::numeric_limits<uint32_t>::max() / 2 - assembly.Faces.size()) {
    for (const auto source : sources) {
      assembly.DeclineProfile(source, WaterAsset::SurfaceRejectionReason::CapacityExceeded);
    }
    return {};
  }
  const auto base = static_cast<uint32_t>(assembly.Vertices.size());
  assembly.Vertices.insert(
      assembly.Vertices.end(), fit->VerticesEnuM.begin(), fit->VerticesEnuM.end());
  for (size_t index = 0; index < body.Surface.Faces.size(); ++index) {
    auto face = body.Surface.Faces[index];
    for (auto &vertex : face.Vertices) { vertex += base; }
    assembly.Faces.push_back(face);
    const auto first = body.Surface.FaceSourceOffsets[index];
    const auto end = body.Surface.FaceSourceOffsets[index + 1];
    assembly.Source(std::span(body.Surface.FaceSources).subspan(first, end - first));
  }
  for (const auto source : sources) { assembly.States[source] = SurfaceState::Prepared; }
  return {};
}

std::expected<void, std::string> AppendRivers(const WaterAsset &water,
                                              const TangentFrame &frame,
                                              const std::stop_token &stop,
                                              SurfaceAssembly &assembly) {
  const auto plan =
      PlanRiverBodies(water, frame, WaterGroundPolicy::AssumeUnlabelledGround, {}, stop);
  if (!plan) { return std::unexpected("river body planning failed or was canceled"); }
  for (const auto &rejection : plan->Rejections) {
    assembly.DeclineProfile(rejection.Surface, rejection.Reason);
  }
  std::vector<uint32_t> owners(water.Surfaces().size());
  for (const auto &body : plan->Bodies) {
    for (const auto source : Sources(body)) { ++owners[source]; }
  }
  for (const auto &body : plan->Bodies) {
    if (stop.stop_requested()) { return std::unexpected("river surface preparation canceled"); }
    const auto sources = Sources(body);
    if (body.Courses.empty()) { continue; }
    if (std::ranges::any_of(sources, [&](uint32_t source) { return owners[source] != 1; })) {
      for (const auto source : sources) {
        assembly.DeclineProfile(source, WaterAsset::SurfaceRejectionReason::InvalidHeightModel);
      }
      continue;
    }
    const auto result = AppendRiver(water, frame, body, stop, assembly);
    if (!result) { return std::unexpected(result.error()); }
  }
  return {};
}

void AppendFlat(const WaterAsset &water,
                uint32_t index,
                const TangentFrame &frame,
                SurfaceAssembly &assembly) {
  const auto &surface = water.Surfaces()[index];
  const auto polygon =
      ReadWaterPolygon(water.RingsOf(surface), water.Points(), surface.LevelM, frame);
  if (!polygon) {
    assembly.Reject(index, WaterAsset::SurfaceRejectionReason::InvalidTopology);
    return;
  }
  if (polygon->Vertices.size() > std::numeric_limits<uint32_t>::max() - assembly.Vertices.size()) {
    assembly.Reject(index, WaterAsset::SurfaceRejectionReason::CapacityExceeded);
    return;
  }
  const auto oriented = TriangulateWaterPolygon(*polygon);
  if (!oriented) {
    assembly.Reject(index, WaterAsset::SurfaceRejectionReason::InvalidTopology);
    return;
  }
  if (oriented->size() / 3 > std::numeric_limits<uint32_t>::max() / 2 - assembly.Faces.size()) {
    assembly.Reject(index, WaterAsset::SurfaceRejectionReason::CapacityExceeded);
    return;
  }
  const auto base = static_cast<uint32_t>(assembly.Vertices.size());
  for (const auto &vertex : polygon->Vertices) {
    assembly.Vertices.push_back({{vertex.EastM, vertex.NorthM, vertex.UpM}});
  }
  for (size_t at = 0; at < oriented->size(); at += 3) {
    assembly.Faces.push_back({.Vertices = {base + (*oriented)[at],
                                           base + (*oriented)[at + 1],
                                           base + (*oriented)[at + 2]},
                              .Surface = index});
    assembly.Source(std::span(&index, 1));
  }
  assembly.States[index] = SurfaceState::Prepared;
}

bool ImplicitSources(const SurfaceAssembly &assembly) {
  for (size_t at = 0; at < assembly.Faces.size(); ++at) {
    const auto first = assembly.Product.FaceSourceOffsets[at];
    const auto end = assembly.Product.FaceSourceOffsets[at + 1];
    if (end - first != 1 || assembly.Product.FaceSources[first] != assembly.Faces[at].Surface) {
      return false;
    }
  }
  return true;
}
}

std::expected<WaterAsset, std::string> PrepareWaterSurfaces(const WaterAsset &water,
                                                            const TangentFrame &frame,
                                                            const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("water surface preparation canceled"); }
  if (water.SurfacesReady()) { return water; }
  SurfaceAssembly assembly(water.Surfaces().size());
  const auto rivers = AppendRivers(water, frame, stop, assembly);
  if (!rivers) { return std::unexpected(rivers.error()); }
  std::vector<uint32_t> ordered(water.Surfaces().size());
  std::ranges::iota(ordered, uint32_t{0});
  std::ranges::sort(ordered, {}, [&water](uint32_t index) {
    const auto rings = water.RingsOf(water.Surfaces()[index]);
    return rings.empty() ? std::numeric_limits<uint32_t>::max() : rings.front().FirstPoint;
  });
  for (const auto index : ordered) {
    if (stop.stop_requested()) { return std::unexpected("water surface preparation canceled"); }
    if (assembly.States[index] == SurfaceState::Pending ||
        assembly.States[index] == SurfaceState::Unprofiled) {
      AppendFlat(water, index, frame, assembly);
    }
  }
  if (ImplicitSources(assembly)) {
    assembly.Product.FaceSourceOffsets.clear();
    assembly.Product.FaceSources.clear();
  }
  auto mesh = WaterSurfaceMesh::Build(
      frame.Anchor(), std::move(assembly.Vertices), std::move(assembly.Faces));
  if (!mesh) { return std::unexpected("water surface mesh is invalid or exceeds its capacity"); }
  assembly.Product.Mesh = std::move(*mesh);
  return water.WithSurfaces(std::move(assembly.Product));
}
}
