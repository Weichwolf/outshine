#include "scene/SurfaceClearance.h"
#include "GeosTriangle.h"

#include <geos_c.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
namespace {

bool Finite(const auto &value) {
  return std::ranges::all_of(value, [](double component) { return std::isfinite(component); });
}

using Geometry = Spatial::GeosGeometry;

struct Face {
  Vec3 Origin;
  Vec2 Gradient;
  size_t Index;
  Geometry Polygon;
};

class Measurement {
public:
  explicit Measurement(SurfaceClearanceLimits limits) : Limits_(limits) {
    if (Context_ != nullptr) {
      GEOSContext_setErrorMessageHandler_r(
          Context_,
          [](const char *, void *failed) { *static_cast<bool *>(failed) = true; },
          &BackendError_);
    }
  }

  Measurement(const Measurement &) = delete;
  Measurement &operator=(const Measurement &) = delete;
  Measurement(Measurement &&) = delete;
  Measurement &operator=(Measurement &&) = delete;

  ~Measurement() {
    if (Tree_ != nullptr) { GEOSSTRtree_destroy_r(Context_, Tree_); }
    Lower_.clear();
    Upper_.clear();
    if (Context_ != nullptr) { GEOS_finish_r(Context_); }
  }

  std::expected<SurfaceClearance, SurfaceClearanceFailure>
  Run(std::array<HeightSurfaceView, 2> surfaces) {
    if (Context_ == nullptr) { return Fail(SurfaceClearanceError::Backend); }
    if (auto status = Prepare(surfaces[0], Lower_); !status) {
      return std::unexpected(status.error());
    }
    if (auto status = Prepare(surfaces[1], Upper_); !status) {
      return std::unexpected(status.error());
    }
    Tree_ = GEOSSTRtree_create_r(Context_, 10);
    if (Tree_ == nullptr) { return Fail(SurfaceClearanceError::Backend); }
    Candidates_.reserve(Upper_.size());
    for (Face &face : Upper_) { GEOSSTRtree_insert_r(Context_, Tree_, face.Polygon.get(), &face); }
    if (BackendError_) { return Fail(SurfaceClearanceError::Backend); }
    for (const Face &face : Lower_) {
      if (Expired()) { return Fail(SurfaceClearanceError::Deadline); }
      Candidates_.clear();
      GEOSSTRtree_query_r(
          Context_,
          Tree_,
          face.Polygon.get(),
          [](void *const faceItem, void *faceRows) {
            static_cast<std::vector<const Face *> *>(faceRows)->push_back(
                static_cast<const Face *>(faceItem));
          },
          &Candidates_);
      if (BackendError_) { return Fail(SurfaceClearanceError::Backend); }
      if (Candidates_.size() > Limits_.MaxCandidateTests - Result_.CandidateTests) {
        return Fail(SurfaceClearanceError::CandidateBudget);
      }
      Result_.CandidateTests += Candidates_.size();
      std::ranges::sort(Candidates_, {}, &Face::Index);
      for (const Face *other : Candidates_) {
        if (Expired()) { return Fail(SurfaceClearanceError::Deadline); }
        if (auto status = Compare(face, *other); !status) {
          return std::unexpected(status.error());
        }
      }
    }
    if (Expired()) { return Fail(SurfaceClearanceError::Deadline); }
    if (Result_.Overlaps == 0) { return Fail(SurfaceClearanceError::NoOverlap); }
    return Result_;
  }

private:
  [[nodiscard]] bool Expired() const {
    return std::chrono::steady_clock::now() >= Limits_.Deadline;
  }

  [[nodiscard]] std::unexpected<SurfaceClearanceFailure> Fail(SurfaceClearanceError error) const {
    return std::unexpected(SurfaceClearanceFailure{.Error = error,
                                                   .CandidateTests = Result_.CandidateTests,
                                                   .Pairs = Result_.Pairs,
                                                   .Overlaps = Result_.Overlaps});
  }

  std::expected<void, SurfaceClearanceFailure> Prepare(HeightSurfaceView input,
                                                       std::vector<Face> &faces) {
    if (input.Vertices.empty() || input.Triangles.empty()) {
      return Fail(SurfaceClearanceError::InvalidInput);
    }
    if (input.Vertices.size() > Limits_.MaxVertices || input.Triangles.size() > Limits_.MaxFaces) {
      return Fail(SurfaceClearanceError::InputBudget);
    }
    if (Expired()) { return Fail(SurfaceClearanceError::Deadline); }
    if (!std::ranges::all_of(input.Vertices,
                             [](const Vec3 &position) { return Finite(position.Axis); })) {
      return Fail(SurfaceClearanceError::InvalidInput);
    }
    faces.reserve(input.Triangles.size());
    for (size_t at = 0; at < input.Triangles.size(); ++at) {
      if (Expired()) { return Fail(SurfaceClearanceError::Deadline); }
      const auto &indices = input.Triangles[at];
      if (std::ranges::any_of(indices,
                              [&](uint32_t index) { return index >= input.Vertices.size(); })) {
        return Fail(SurfaceClearanceError::InvalidInput);
      }
      const std::array points{
          input.Vertices[indices[0]], input.Vertices[indices[1]], input.Vertices[indices[2]]};
      const Vec3 normal = Cross(points[1] - points[0], points[2] - points[0]);
      if (!Finite(normal.Axis)) { return Fail(SurfaceClearanceError::Unrepresentable); }
      if (normal[2] == 0) { return Fail(SurfaceClearanceError::InvalidInput); }
      const Vec2 gradient{{-normal[0] / normal[2], -normal[1] / normal[2]}};
      if (!Finite(gradient.Axis)) { return Fail(SurfaceClearanceError::Unrepresentable); }
      auto polygon = Spatial::ProjectedTriangle(Context_, points);
      if (!polygon || BackendError_) { return Fail(SurfaceClearanceError::Backend); }
      faces.push_back(
          {.Origin = points[0], .Gradient = gradient, .Index = at, .Polygon = std::move(polygon)});
    }
    return {};
  }

  std::expected<void, SurfaceClearanceFailure> Compare(const Face &lower, const Face &upper) {
    const char intersects = GEOSIntersects_r(Context_, lower.Polygon.get(), upper.Polygon.get());
    if (intersects == 2 || BackendError_) { return Fail(SurfaceClearanceError::Backend); }
    if (intersects == 0) { return {}; }
    if (Result_.Pairs == Limits_.MaxPairs) { return Fail(SurfaceClearanceError::PairBudget); }
    ++Result_.Pairs;
    const Geometry overlap(GEOSIntersection_r(Context_, lower.Polygon.get(), upper.Polygon.get()),
                           {Context_});
    double area = 0;
    if (!overlap || GEOSArea_r(Context_, overlap.get(), &area) == 0 || BackendError_) {
      return Fail(SurfaceClearanceError::Backend);
    }
    if (!std::isfinite(area)) { return Fail(SurfaceClearanceError::Unrepresentable); }
    if (!(area > 0)) { return {}; }
    ++Result_.Overlaps;
    const GEOSGeometry *ring = GEOSGetExteriorRing_r(Context_, overlap.get());
    const GEOSCoordSequence *coordinates =
        ring == nullptr ? nullptr : GEOSGeom_getCoordSeq_r(Context_, ring);
    unsigned count = 0;
    if (coordinates == nullptr || GEOSCoordSeq_getSize_r(Context_, coordinates, &count) == 0 ||
        count < 4) {
      return Fail(SurfaceClearanceError::Backend);
    }
    for (unsigned at = 0; at < count; ++at) {
      Vec2 xy;
      if (GEOSCoordSeq_getXY_r(Context_, coordinates, at, xy.data(), &xy[1]) == 0) {
        return Fail(SurfaceClearanceError::Backend);
      }
      const auto height = [&](const Face &face) {
        return face.Origin[2] + (xy[0] - face.Origin[0]) * face.Gradient[0] +
               (xy[1] - face.Origin[1]) * face.Gradient[1];
      };
      const double gap = height(upper) - height(lower);
      if (!std::isfinite(gap)) { return Fail(SurfaceClearanceError::Unrepresentable); }
      if ((Result_.Overlaps == 1 && at == 0) || gap < Result_.MinimumM) {
        Result_.MinimumM = gap;
        Result_.Witness = xy;
        Result_.LowerFace = lower.Index;
        Result_.UpperFace = upper.Index;
      }
    }
    if (BackendError_) { return Fail(SurfaceClearanceError::Backend); }
    return {};
  }

  SurfaceClearanceLimits Limits_;
  GEOSContextHandle_t Context_ = GEOS_init_r();
  GEOSSTRtree *Tree_ = nullptr;
  bool BackendError_ = false;
  std::vector<Face> Lower_, Upper_;
  std::vector<const Face *> Candidates_;
  SurfaceClearance Result_;
};
}

std::expected<SurfaceClearance, SurfaceClearanceFailure> MeasureSurfaceClearance(
    HeightSurfaceView lower, HeightSurfaceView upper, SurfaceClearanceLimits limits) {
  return Measurement(limits).Run({lower, upper});
}
}
