#ifndef OUTSHINE_SCENE_SURFACESAMPLER_H
#define OUTSHINE_SCENE_SURFACESAMPLER_H

#include "math/Vec2.h"
#include "math/Vec3.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <vector>

namespace outshine {

/// Preparation/query failures; failed preparation publishes no partial sampler.
enum class SurfaceSamplerError { InvalidInput, InputBudget, Unrepresentable, Backend };
/// Local contact outcome; unresolved contacts retain NaN height/normal and face -1.
enum class SurfaceContactStatus { Contact, Outside, Ambiguous, CandidateBudget };

/// Explicit preparation bounds and metric tolerances in the caller's local frame.
struct SurfaceSamplerLimits {
  /// Default metric radius around projected triangle boundaries.
  static constexpr double DefaultXyToleranceM = 1e-8;
  /// Default permitted height difference at a shared topological contact.
  static constexpr double DefaultHeightToleranceM = 1e-7;
  /// Maximum vertices copied before indexing.
  size_t MaxVertices = size_t{3} * 65536;
  /// Maximum triangles prepared, including overlapping projections.
  size_t MaxFaces = 65536;
  /// Projected metric distance allowed outside triangle boundaries.
  double XyToleranceM = DefaultXyToleranceM;
  /// Height agreement required at a shared topological boundary.
  double HeightToleranceM = DefaultHeightToleranceM;
};

/// Bounded batch request; candidate budget counts exact projected face matches.
struct SurfaceQueryLimits {
  /// Default cap on emitted projected matches in one request.
  static constexpr size_t DefaultMaxCandidates = 1000000;
  /// Maximum positions accepted before allocating result storage.
  size_t MaxQueries = 65536;
  /// Maximum exact projected face matches across the request.
  size_t MaxCandidates = DefaultMaxCandidates;
};

/// One sample on the explicitly chosen mesh, with its upward facet normal.
struct SurfaceContact {
  /// Input XY with affine height, or NaN height when unresolved.
  Vec3 PositionM;
  /// Upward unit facet normal, or all NaN when unresolved.
  Vec3 Normal;
  /// Original triangle index, or -1 when unresolved.
  int64_t FaceIndex = -1;
  /// Contact or the local reason no contact could be selected.
  SurfaceContactStatus Status = SurfaceContactStatus::Outside;
};

/// Owned batch result. Counters exclude preparation and count completed query batches.
struct SurfaceSamples {
  /// Samples in the same order as the input positions.
  std::vector<SurfaceContact> Contacts;
  /// Exact projected matches examined, including ambiguous contacts.
  size_t Candidates = 0;
  /// Completed conservative query batches, excluding omitted queries.
  size_t Batches = 0;
};

/// Prepared immutable local XY height surface. Owns its mesh/index; no IO or source semantics.
/// The caller binds frame, ground/deck/roof meaning and face provenance. Serialize queries
/// on the same sampler; independent samplers can be used concurrently. May allocate.
class SurfaceSampler {
public:
  /// Transfer owned mesh/index; the moved-from object cannot sample.
  SurfaceSampler(SurfaceSampler &&) noexcept;
  /// Release current mesh/index and take ownership from another sampler.
  /// @return This sampler with transferred ownership.
  SurfaceSampler &operator=(SurfaceSampler &&) noexcept;
  ~SurfaceSampler();

  /// Copy and index a finite mesh within explicit caps. Vertical/degenerate XY faces and
  /// duplicate topological triangles are invalid. Winding does not change upward normals.
  /// @param vertices Local metric mesh positions; XY is the height projection plane.
  /// @param triangles Topological vertex indices, retained as output face identities.
  /// @param limits Preparation caps and metric boundary tolerances.
  /// @return Owned prepared sampler, or explicit input, budget, numeric or backend error.
  [[nodiscard]] static std::expected<SurfaceSampler, SurfaceSamplerError>
  prepare(std::span<const Vec3> vertices,
          std::span<const std::array<uint32_t, 3>> triangles,
          SurfaceSamplerLimits limits = {});

  /// Sample finite XY positions. Consistent shared edges/vertices choose the lowest face ID;
  /// other multiple hits remain ambiguous. Affine planes extrapolate only within XY tolerance.
  /// Budget exhaustion preserves earlier contacts and marks remaining queries unresolved.
  /// Conservative batches require room for every face before emitting candidates.
  /// @param xy Finite local metric positions in the prepared mesh frame.
  /// @param limits Query and exact candidate bounds for this batch request.
  /// @return Ordered contacts and counters, or explicit input, numeric or backend error.
  [[nodiscard]] std::expected<SurfaceSamples, SurfaceSamplerError>
  sample(std::span<const Vec2> xy, SurfaceQueryLimits limits = {}) const;

private:
  class State;
  explicit SurfaceSampler(std::unique_ptr<State> state);
  std::unique_ptr<State> State_;
};

}
#endif
