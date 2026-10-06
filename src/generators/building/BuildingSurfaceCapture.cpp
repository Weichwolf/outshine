#include "BuildingSurfaceCapture.h"
#include "BuildingSurface.h"
#include "BuildingScratch.h"
#include "BuildingMesh.h"
#include "PlanHierarchy.h"
#include "PlaneSurfacePatches.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>
#include <limits>
#include <optional>
#include <vector>

namespace outshine::Generators {
namespace {

constexpr uint32_t kEmptyPixel = std::numeric_limits<uint32_t>::max();

struct CubeFace {
  Vec3 Forward, Right, Up;
  int Size = 0;

  [[nodiscard]] Vec3 Ray(double x, double y) const {
    return Forward + Right * (2.0 * x / Size - 1.0) + Up * (2.0 * y / Size - 1.0);
  }
};

struct Region {
  int Left = 0, Top = 0, Right = 0, Bottom = 0;
};

Region Project(const Box &bounds, const Vec3 &eye, const CubeFace &face) {
  double near = std::numeric_limits<double>::infinity();
  double far = -near;
  double x0 = near;
  double y0 = near;
  double x1 = far;
  double y1 = far;
  std::array<double, 4> planeMaximum;
  planeMaximum.fill(-std::numeric_limits<double>::infinity());
  for (unsigned at = 0; at < 8; ++at) {
    const Vec3 point = bounds.Corner(at) - eye;
    const double depth = Dot(point, face.Forward);
    const double right = Dot(point, face.Right);
    const double up = Dot(point, face.Up);
    const std::array sides{depth - right, depth + right, depth - up, depth + up};
    for (size_t plane = 0; plane < sides.size(); ++plane) {
      planeMaximum[plane] = std::max(planeMaximum[plane], sides[plane]);
    }
    near = std::min(near, depth);
    far = std::max(far, depth);
    if (depth <= 0.0) { continue; }
    const double x = Dot(point, face.Right) / depth;
    const double y = Dot(point, face.Up) / depth;
    x0 = std::min(x0, x);
    x1 = std::max(x1, x);
    y0 = std::min(y0, y);
    y1 = std::max(y1, y);
  }
  if (far <= 0.0 ||
      std::ranges::any_of(planeMaximum, [](double maximum) { return maximum < 0.0; })) {
    return {};
  }
  if (near <= 0.0) { return {.Left = 0, .Top = 0, .Right = face.Size, .Bottom = face.Size}; }
  const auto lower = [&](double value) {
    return std::clamp(
        static_cast<int>(std::floor((std::clamp(value, -1.0, 1.0) + 1.0) * 0.5 * face.Size)) - 1,
        0,
        face.Size);
  };
  const auto upper = [&](double value) {
    return std::clamp(
        static_cast<int>(std::ceil((std::clamp(value, -1.0, 1.0) + 1.0) * 0.5 * face.Size)) + 1,
        0,
        face.Size);
  };
  if (x0 > 1.0 || x1 < -1.0 || y0 > 1.0 || y1 < -1.0) { return {}; }
  return {.Left = lower(x0), .Top = lower(y0), .Right = upper(x1), .Bottom = upper(y1)};
}

std::vector<Region> OccupiedRegions(std::span<const BuildingSurface> sources,
                                    std::span<const StructurePlan> plans,
                                    const CubeFace &face,
                                    const Vec3 &eye) {
  constexpr int kTilePixels = 64;
  const int width = (face.Size + kTilePixels - 1) / kTilePixels;
  std::vector<uint8_t> occupied(static_cast<size_t>(width) * width);
  for (size_t source = 0; source < sources.size(); ++source) {
    if (plans[source].Coarseness == LevelOfDetail::Fine || !sources[source].SupportsProjection()) {
      continue;
    }
    const Region region = Project(sources[source].Bounds(), eye, face);
    if (region.Left == region.Right || region.Top == region.Bottom) { continue; }
    for (int y = region.Top / kTilePixels; y <= (region.Bottom - 1) / kTilePixels; ++y) {
      for (int x = region.Left / kTilePixels; x <= (region.Right - 1) / kTilePixels; ++x) {
        occupied[static_cast<size_t>(y) * width + x] = 1;
      }
    }
  }
  std::vector<Region> regions;
  for (int y = 0; y < width; ++y) {
    for (int x = 0; x < width; ++x) {
      if (occupied[static_cast<size_t>(y) * width + x] == 0) { continue; }
      regions.push_back({.Left = x * kTilePixels,
                         .Top = y * kTilePixels,
                         .Right = std::min((x + 1) * kTilePixels, face.Size),
                         .Bottom = std::min((y + 1) * kTilePixels, face.Size)});
    }
  }
  return regions;
}

void VisibleFaces(const PlanHierarchy &hierarchy,
                  std::span<const BuildingSurface> sources,
                  const CubeFace &face,
                  const Region &region,
                  const Vec3 &eye,
                  std::vector<std::vector<uint8_t>> &visible,
                  std::vector<std::vector<BuildingSurfacePatch>> &captured,
                  std::vector<uint8_t> &invalid) {
  const auto width = static_cast<uint32_t>(region.Right - region.Left);
  const auto height = static_cast<uint32_t>(region.Bottom - region.Top);
  std::vector<PlaneSurfaceSample> samples(static_cast<size_t>(width) * height);
  std::vector<double> cuts;
  cuts.reserve(32);
  for (int y = region.Top; y < region.Bottom; ++y) {
    for (int x = region.Left; x < region.Right; ++x) {
      uint32_t nearest = kEmptyPixel;
      BuildingSurface::Hit nearestHit;
      const Vec3 direction = face.Ray(x + 0.5, y + 0.5);
      (void)hierarchy.TraceClosest(
          eye,
          direction,
          0.0,
          std::numeric_limits<double>::infinity(),
          [&](uint32_t source, double maximum) -> std::optional<double> {
            if (!sources[source].SupportsProjection()) { return std::nullopt; }
            const auto hit =
                sources[source].Trace({.Origin = eye, .Direction = direction}, 0.0, maximum, cuts);
            if (!hit || Dot(hit->Normal, direction) >= 0.0) { return std::nullopt; }
            nearestHit = *hit;
            nearest = source;
            return hit->Along;
          });
      if (nearest == kEmptyPixel) { continue; }
      const size_t surface = sources[nearest].FaceIndex(nearestHit);
      visible[nearest][surface] = 1;
      const uint64_t code = (static_cast<uint64_t>(nearest) << 32) |
                            (static_cast<uint64_t>(surface) << 1) |
                            (nearestHit.Gable ? uint64_t{1} : uint64_t{0});
      samples[static_cast<size_t>(y - region.Top) * width + x - region.Left] = {
          .Surface = code + 1,
          .Point = eye + direction * nearestHit.Along,
          .Normal = nearestHit.Normal};
    }
  }
  const auto patches = BuildPlaneSurfacePatches(samples, width, height);
  if (!patches) {
    std::ranges::fill(invalid, uint8_t{1});
    return;
  }
  for (const auto &patch : *patches) {
    const auto &sample = samples[patch.Sample];
    const uint64_t code = sample.Surface - 1;
    const auto owner = static_cast<uint32_t>(code >> 32);
    const auto id = static_cast<uint32_t>(code);
    const auto nativeFace = sources[owner].FaceAt(id >> 1);
    const std::array directions{face.Ray(region.Left + patch.Left, region.Top + patch.Top),
                                face.Ray(region.Left + patch.Right, region.Top + patch.Top),
                                face.Ray(region.Left + patch.Right, region.Top + patch.Bottom),
                                face.Ray(region.Left + patch.Left, region.Top + patch.Bottom)};
    const auto corners = IntersectPlaneSurface(sample, eye, directions);
    if (!corners) {
      invalid[owner] = 1;
      continue;
    }
    captured[owner].push_back({.Sample = {.Normal = sample.Normal,
                                          .Part = nativeFace.Part,
                                          .Face = nativeFace.Side,
                                          .Gable = (id & 1U) != 0},
                               .Corners = *corners});
  }
}

std::expected<void, StructureMeshError>
EmitSurfaces(std::span<const BuildingSurface> sources,
             std::span<const StructurePlan> plans,
             const std::vector<std::vector<uint8_t>> &visible,
             const std::vector<std::vector<BuildingSurfacePatch>> &captured,
             std::span<const uint8_t> invalid,
             BuildingScratch &scratch,
             Raised &output) {
  for (size_t source = 0; source < sources.size(); ++source) {
    if (plans[source].Coarseness == LevelOfDetail::Fine || !sources[source].SupportsProjection()) {
      const auto emitted = BuildingMesh{}.Mesh(plans[source], scratch, output);
      if (!emitted) { return std::unexpected(emitted.error()); }
      continue;
    }
    std::vector<BuildingSurface::Face> faces;
    for (size_t face = 0; face < visible[source].size(); ++face) {
      if (visible[source][face] != 0) { faces.push_back(sources[source].FaceAt(face)); }
    }
    if (invalid[source] == 0 && !captured[source].empty() &&
        captured[source].size() * 2 < sources[source].EstimatedTriangles(faces)) {
      sources[source].MeshPatches(captured[source], output);
      continue;
    }
    const auto emitted = sources[source].MeshVisible(faces, scratch, output);
    if (!emitted) { return std::unexpected(emitted.error()); }
  }
  return {};
}

}

std::expected<ProjectedStructureMesh, StructureMeshError>
CaptureBuildingSurfaces(std::span<const StructurePlan> plans,
                        const Vec3 &eye,
                        ProjectedErrorBudget projection,
                        BuildingScratch &scratch) {
  if (plans.empty() || !std::isfinite(projection.FocalPx) || projection.FocalPx <= 0.0 ||
      !std::isfinite(projection.AllowedErrorPx) || projection.AllowedErrorPx <= 0.0) {
    return std::unexpected(StructureMeshError::InvalidPlan);
  }
  const double size = std::ceil(4.0 * projection.FocalPx / projection.AllowedErrorPx);
  if (size > std::numeric_limits<int>::max()) {
    return std::unexpected(StructureMeshError::InvalidPlan);
  }
  if (plans.size() > std::numeric_limits<uint32_t>::max() ||
      !std::ranges::all_of(eye, [](double value) { return std::isfinite(value); })) {
    return std::unexpected(StructureMeshError::InvalidPlan);
  }
  std::vector<BuildingSurface> sources;
  std::vector<Box> bounds;
  sources.reserve(plans.size());
  bounds.reserve(plans.size());
  ProjectedStructureMesh output;
  output.EyeRadiusM = 0.0;
  for (const auto &plan : plans) {
    auto source = BuildingSurface::Prepare(plan, scratch);
    if (!source) { return std::unexpected(source.error()); }
    if (source->FaceCount() > std::numeric_limits<uint32_t>::max() / 2) {
      return std::unexpected(StructureMeshError::InvalidPlan);
    }
    bounds.push_back(source->Bounds());
    sources.push_back(std::move(*source));
  }
  const auto hierarchy = PlanHierarchy::Build(bounds);
  if (!hierarchy || hierarchy->Nodes().empty()) {
    return std::unexpected(StructureMeshError::InvalidPlan);
  }
  const int dimension = static_cast<int>(size);
  std::vector<std::vector<BuildingSurfacePatch>> captured(sources.size());
  std::vector<uint8_t> invalid(sources.size());
  std::vector<std::vector<uint8_t>> visible;
  visible.reserve(sources.size());
  for (const auto &source : sources) { visible.emplace_back(source.FaceCount()); }
  for (size_t axis = 0; axis < 3; ++axis) {
    for (const double sign : {-1.0, 1.0}) {
      CubeFace face{.Forward = {}, .Right = {}, .Up = {}, .Size = dimension};
      face.Forward[axis] = sign;
      face.Right[(axis + 1) % 3] = 1;
      face.Up[(axis + 2) % 3] = 1;
      for (const Region &region : OccupiedRegions(sources, plans, face, eye)) {
        VisibleFaces(*hierarchy, sources, face, region, eye, visible, captured, invalid);
      }
    }
  }
  const auto emitted =
      EmitSurfaces(sources, plans, visible, captured, invalid, scratch, output.Mesh);
  if (!emitted) { return std::unexpected(emitted.error()); }

  return output;
}

std::expected<ProjectedStructureMesh, StructureMeshError>
BuildingMesh::Project(std::span<const StructurePlan> plans,
                      const Vec3 &eye,
                      ProjectedErrorBudget projection,
                      MeshScratch &lent) const {
  auto *scratch = dynamic_cast<BuildingScratch *>(&lent);
  if (scratch == nullptr) { return std::unexpected(StructureMeshError::IncompatibleScratch); }
  return CaptureBuildingSurfaces(plans, eye, projection, *scratch);
}

}
