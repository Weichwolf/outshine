#ifndef OUTSHINE_SCENE_SURFACECLEARANCE_H
#define OUTSHINE_SCENE_SURFACECLEARANCE_H

#include "math/Vec2.h"
#include "math/Vec3.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace outshine {

/// Borrowed local XYZ height surface; XY is horizontal and Z is height in meters.
struct HeightSurfaceView {
  /// Finite positions, retained only for this call.
  std::span<const Vec3> Vertices;
  /// Original face indices; winding does not select surface meaning.
  std::span<const std::array<uint32_t, 3>> Triangles;
};

/// Independent preparation, spatial-query, exact-pair and cooperative time bounds.
struct SurfaceClearanceLimits {
  /// Default work cap independent of the exact-intersection budget.
  static constexpr size_t DefaultMaxCandidateTests = 1000000;
  /// Maximum positions on either selected surface.
  size_t MaxVertices = size_t{3} * 65536;
  /// Maximum faces on either selected surface.
  size_t MaxFaces = 8192;
  /// Maximum bounding-box candidates examined across the call.
  size_t MaxCandidateTests = DefaultMaxCandidateTests;
  /// Maximum exact projected intersections, including point/edge contacts.
  size_t MaxPairs = 65536;
  /// Caller-owned worker deadline; never invoke blocking geometry work in a frame.
  std::chrono::steady_clock::time_point Deadline = std::chrono::steady_clock::time_point::max();
};

/// Reasons no complete minimum can be published.
enum class SurfaceClearanceError {
  InvalidInput,
  InputBudget,
  CandidateBudget,
  PairBudget,
  Deadline,
  Unrepresentable,
  NoOverlap,
  Backend
};

/// Diagnostic work counts; failure never contains a partial numeric minimum.
struct SurfaceClearanceFailure {
  /// Input, numeric, budget or backend failure.
  SurfaceClearanceError Error;
  /// Bounding-box candidates examined before failure.
  size_t CandidateTests = 0;
  /// Exact projected intersections examined before failure.
  size_t Pairs = 0;
  /// Positive-area overlaps examined before failure.
  size_t Overlaps = 0;
};

/// Complete signed upper-minus-lower height minimum over positive XY overlap.
struct SurfaceClearance {
  /// Signed gap; a negative value is valid overlap, not a physical policy decision.
  double MinimumM = 0;
  /// Local XY witness where both selected affine faces have the reported gap.
  Vec2 Witness;
  /// Original face on the lower input.
  size_t LowerFace = 0;
  /// Original face on the upper input.
  size_t UpperFace = 0;
  /// Bounding-box candidates examined.
  size_t CandidateTests = 0;
  /// Exact projected intersections examined, including point/edge contacts.
  size_t Pairs = 0;
  /// Positive-area overlaps included in the complete minimum.
  size_t Overlaps = 0;
};

/// Measure explicitly selected surfaces in one caller-bound frame/datum.
/// Preparation owns bounded temporary geometry; inputs remain unchanged.
/// @param lower The selected lower height surface, not an implicit ground query.
/// @param upper The selected upper/underside surface, not an automatic floor choice.
/// @param limits Independent work bounds and a cooperative worker deadline.
/// @return Complete minimum with witness/face IDs, or failure without a partial minimum.
[[nodiscard]] std::expected<SurfaceClearance, SurfaceClearanceFailure> MeasureSurfaceClearance(
    HeightSurfaceView lower, HeightSurfaceView upper, SurfaceClearanceLimits limits = {});

}
#endif
