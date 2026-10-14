#include "scene/SurfaceSampler.h"
#include <geos_c.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
namespace {
bool Finite(const auto &value) {
  return std::ranges::all_of(value, [](double component) { return std::isfinite(component); });
}

SurfaceContact Missing(Vec2 xy, SurfaceContactStatus status = SurfaceContactStatus::Outside) {
  const double absent = std::numeric_limits<double>::quiet_NaN();
  return {.PositionM = {{xy[0], xy[1], absent}},
          .Normal = {{absent, absent, absent}},
          .FaceIndex = -1,
          .Status = status};
}

struct DestroyGeometry {
  GEOSContextHandle_t Context;

  void operator()(GEOSGeometry *geometry) const { GEOSGeom_destroy_r(Context, geometry); }
};

using Geometry = std::unique_ptr<GEOSGeometry, DestroyGeometry>;

Geometry Triangle(GEOSContextHandle_t context, const std::array<Vec3, 3> &corners) {
  GEOSCoordSequence *coordinates = GEOSCoordSeq_create_r(context, 4, 2);
  if (coordinates == nullptr) { return {nullptr, {context}}; }
  for (unsigned at = 0; at < 4; ++at) {
    if (GEOSCoordSeq_setXY_r(context, coordinates, at, corners[at % 3][0], corners[at % 3][1]) ==
        0) {
      GEOSCoordSeq_destroy_r(context, coordinates);
      return {nullptr, {context}};
    }
  }
  GEOSGeometry *ring = GEOSGeom_createLinearRing_r(context, coordinates);
  if (ring == nullptr) { return {nullptr, {context}}; }
  return {GEOSGeom_createPolygon_r(context, ring, nullptr, 0), {context}};
}
}

class SurfaceSampler::State {
public:
  struct Face {
    std::array<uint32_t, 3> Vertices;
    Vec2 Gradient;
    Vec3 Normal;
    Geometry Polygon;
    size_t Index;
  };

  explicit State(SurfaceSamplerLimits limits) : Limits(limits) {
    Context = GEOS_init_r();
    if (Context == nullptr) { return; }
    GEOSContext_setErrorMessageHandler_r(
        Context, [](const char *, void *flag) { *static_cast<bool *>(flag) = true; }, &Error);
    Tree = GEOSSTRtree_create_r(Context, 10);
  }

  ~State() {
    if (Tree != nullptr) { GEOSSTRtree_destroy_r(Context, Tree); }
    Faces.clear();
    if (Context != nullptr) { GEOS_finish_r(Context); }
  }

  std::expected<void, SurfaceSamplerError> append(std::array<uint32_t, 3> indices, size_t index) {
    std::array<Vec3, 3> corners;
    for (size_t at = 0; at < 3; ++at) { corners[at] = Vertices[indices[at]]; }
    const Vec3 normal = Cross(corners[1] - corners[0], corners[2] - corners[0]);
    if (!Finite(normal)) { return std::unexpected(SurfaceSamplerError::Unrepresentable); }
    if (normal[2] == 0.0) { return std::unexpected(SurfaceSamplerError::InvalidInput); }
    const Vec2 gradient{{-normal[0] / normal[2], -normal[1] / normal[2]}};
    const double magnitude = std::hypot(std::hypot(gradient[0], gradient[1]), 1.0);
    const Vec3 up{{-gradient[0] / magnitude, -gradient[1] / magnitude, 1.0 / magnitude}};
    if (!Finite(gradient) || !std::isfinite(magnitude) || !Finite(up)) {
      return std::unexpected(SurfaceSamplerError::Unrepresentable);
    }
    Geometry polygon = Triangle(Context, corners);
    if (!polygon || Error) { return std::unexpected(SurfaceSamplerError::Backend); }
    Faces.push_back({.Vertices = indices,
                     .Gradient = gradient,
                     .Normal = up,
                     .Polygon = std::move(polygon),
                     .Index = index});
    GEOSSTRtree_insert_r(Context, Tree, Faces.back().Polygon.get(), &Faces.back());
    if (Error) { return std::unexpected(SurfaceSamplerError::Backend); }
    return {};
  }

  std::array<int64_t, 2> feature(const Face &face, Vec2 xy) const {
    std::array<double, 3> vertexDistances;
    std::array<double, 3> edgeDistances;
    for (size_t at = 0; at < 3; ++at) {
      const Vec3 &p = Vertices[face.Vertices[at]];
      const Vec3 &q = Vertices[face.Vertices[(at + 1) % 3]];
      const double dx = xy[0] - p[0];
      const double dy = xy[1] - p[1];
      vertexDistances[at] = std::hypot(dx, dy);
      edgeDistances[at] =
          std::abs((q[0] - p[0]) * dy - (q[1] - p[1]) * dx) / std::hypot(q[0] - p[0], q[1] - p[1]);
    }
    const size_t vertex = std::ranges::min_element(vertexDistances) - vertexDistances.begin();
    if (vertexDistances[vertex] <= Limits.XyToleranceM) {
      return {face.Vertices[vertex], face.Vertices[vertex]};
    }
    const size_t edge = std::ranges::min_element(edgeDistances) - edgeDistances.begin();
    if (edgeDistances[edge] <= Limits.XyToleranceM) {
      std::array<int64_t, 2> ids{face.Vertices[edge], face.Vertices[(edge + 1) % 3]};
      std::ranges::sort(ids);
      return ids;
    }
    const int64_t own = -static_cast<int64_t>(face.Index) - 1;
    return {own, own};
  }

  std::expected<SurfaceContact, SurfaceSamplerError>
  contact(Vec2 xy, size_t &candidates, std::vector<const Face *> &found) const {
    Error = false;
    const double tolerance = Limits.XyToleranceM;
    const Geometry point(GEOSGeom_createPointFromXY_r(Context, xy[0], xy[1]), {Context});
    const Vec2 lower{{xy[0] - tolerance, xy[1] - tolerance}};
    const Vec2 upper{{xy[0] + tolerance, xy[1] + tolerance}};
    if (!Finite(lower) || !Finite(upper)) {
      return std::unexpected(SurfaceSamplerError::Unrepresentable);
    }
    const Geometry box(tolerance == 0 ? GEOSGeom_createPointFromXY_r(Context, xy[0], xy[1])
                                      : GEOSGeom_createRectangle_r(
                                            Context, lower[0], lower[1], upper[0], upper[1]),
                       {Context});
    if (!point || !box || Error) { return std::unexpected(SurfaceSamplerError::Backend); }
    found.clear();
    GEOSSTRtree_query_r(
        Context,
        Tree,
        box.get(),
        [](void *const faceItem, void *faceRows) {
          static_cast<std::vector<const Face *> *>(faceRows)->push_back(
              static_cast<const Face *>(faceItem));
        },
        &found);
    if (Error) { return std::unexpected(SurfaceSamplerError::Backend); }
    std::ranges::sort(found, {}, &Face::Index);
    SurfaceContact result = Missing(xy);
    std::array<int64_t, 2> firstFeature{};
    double low = 0.0;
    double high = 0.0;
    bool shared = true;
    for (const Face *face : found) {
      const char near = GEOSDistanceWithin_r(Context, face->Polygon.get(), point.get(), tolerance);
      if (near == 2 || Error) { return std::unexpected(SurfaceSamplerError::Backend); }
      if (near == 0) { continue; }
      ++candidates;
      const Vec3 &p = Vertices[face->Vertices[0]];
      const double height =
          p[2] + (xy[0] - p[0]) * face->Gradient[0] + (xy[1] - p[1]) * face->Gradient[1];
      if (!std::isfinite(height)) { shared = false; }
      const auto key = feature(*face, xy);
      if (result.FaceIndex < 0) {
        result = {.PositionM = {{xy[0], xy[1], height}},
                  .Normal = face->Normal,
                  .FaceIndex = static_cast<int64_t>(face->Index),
                  .Status = SurfaceContactStatus::Contact};
        firstFeature = key;
        low = high = height;
      } else {
        shared = shared && key == firstFeature;
        low = std::min(low, height);
        high = std::max(high, height);
      }
    }
    if (result.FaceIndex >= 0 && (!shared || !std::isfinite(low) || !std::isfinite(high) ||
                                  high - low > Limits.HeightToleranceM)) {
      return Missing(xy, SurfaceContactStatus::Ambiguous);
    }
    return result;
  }

  SurfaceSamplerLimits Limits;
  std::vector<Vec3> Vertices;
  std::vector<Face> Faces;
  GEOSContextHandle_t Context = nullptr;
  GEOSSTRtree *Tree = nullptr;
  mutable bool Error = false;
};

SurfaceSampler::SurfaceSampler(std::unique_ptr<State> state) : State_(std::move(state)) {}

SurfaceSampler::SurfaceSampler(SurfaceSampler &&) noexcept = default;
SurfaceSampler &SurfaceSampler::operator=(SurfaceSampler &&) noexcept = default;
SurfaceSampler::~SurfaceSampler() = default;

std::expected<SurfaceSampler, SurfaceSamplerError>
SurfaceSampler::prepare(std::span<const Vec3> vertices,
                        std::span<const std::array<uint32_t, 3>> triangles,
                        SurfaceSamplerLimits limits) {
  if (limits.MaxFaces == 0 || limits.MaxVertices == 0 || !std::isfinite(limits.XyToleranceM) ||
      limits.XyToleranceM < 0.0 || !std::isfinite(limits.HeightToleranceM) ||
      limits.HeightToleranceM < 0.0) {
    return std::unexpected(SurfaceSamplerError::InvalidInput);
  }
  if (vertices.size() > limits.MaxVertices || triangles.size() > limits.MaxFaces) {
    return std::unexpected(SurfaceSamplerError::InputBudget);
  }
  if (!std::ranges::all_of(vertices, [](const Vec3 &p) { return Finite(p); })) {
    return std::unexpected(SurfaceSamplerError::InvalidInput);
  }
  auto state = std::make_unique<State>(limits);
  if (state->Tree == nullptr) { return std::unexpected(SurfaceSamplerError::Backend); }
  state->Vertices.assign(vertices.begin(), vertices.end());
  state->Faces.reserve(triangles.size());
  std::set<std::array<uint32_t, 3>> unique;
  for (size_t at = 0; at < triangles.size(); ++at) {
    auto ids = triangles[at];
    if (std::ranges::any_of(ids, [&](uint32_t id) { return id >= vertices.size(); })) {
      return std::unexpected(SurfaceSamplerError::InvalidInput);
    }
    std::ranges::sort(ids);
    if (!unique.insert(ids).second) { return std::unexpected(SurfaceSamplerError::InvalidInput); }
    if (const auto added = state->append(triangles[at], at); !added) {
      return std::unexpected(added.error());
    }
  }
  if (GEOSSTRtree_build_r(state->Context, state->Tree) == 0 || state->Error) {
    return std::unexpected(SurfaceSamplerError::Backend);
  }
  return SurfaceSampler(std::move(state));
}

std::expected<SurfaceSamples, SurfaceSamplerError>
SurfaceSampler::sample(std::span<const Vec2> xy, SurfaceQueryLimits limits) const {
  if (!State_ || limits.MaxQueries == 0 || xy.size() > limits.MaxQueries) {
    return std::unexpected(SurfaceSamplerError::InputBudget);
  }
  if (!std::ranges::all_of(xy, [](Vec2 point) { return Finite(point); })) {
    return std::unexpected(SurfaceSamplerError::InvalidInput);
  }
  SurfaceSamples result;
  result.Contacts.reserve(xy.size());
  for (const Vec2 point : xy) { result.Contacts.push_back(Missing(point)); }
  std::vector<const State::Face *> scratch;
  scratch.reserve(State_->Faces.size());
  size_t begin = 0;
  while (begin < xy.size() && !State_->Faces.empty()) {
    const size_t count =
        std::min({size_t{256},
                  (limits.MaxCandidates - result.Candidates) / State_->Faces.size(),
                  xy.size() - begin});
    if (count == 0) {
      for (size_t at = begin; at < xy.size(); ++at) {
        result.Contacts[at].Status = SurfaceContactStatus::CandidateBudget;
      }
      break;
    }
    for (size_t at = begin; at < begin + count; ++at) {
      auto contact = State_->contact(xy[at], result.Candidates, scratch);
      if (!contact) { return std::unexpected(contact.error()); }
      result.Contacts[at] = *contact;
    }
    ++result.Batches;
    begin += count;
  }
  return result;
}

}
