#ifndef OUTSHINE_LOADED_H
#define OUTSHINE_LOADED_H

#include <memory>
#include <expected>
#include "Extent.h"
#include <span>
#include <string>
#include <string_view>

#include "Geometry.h"
#include "scenario/Scenario.h"

namespace outshine {

/// Owning glTF/GLB import adapter exposing native geometry and cameras.
/// Loading and pose evaluation may allocate and perform substantial CPU work; keep them
/// outside the frame hot path. Serialize all access, including reads of borrowed data.
/// Borrowed geometry/camera data expires on successful mutation, move or destruction.
class Loaded {
public:
  Loaded();
  ~Loaded();
  Loaded(Loaded &&) noexcept;
  Loaded &operator=(Loaded &&) noexcept;
  Loaded(const Loaded &) = delete;
  Loaded &operator=(const Loaded &) = delete;

  /// Load and convert a complete asset before replacing the current one.
  /// @param path Borrowed filesystem path; external resources resolve relative to the asset.
  /// @return Success, or an owned diagnostic. Failure preserves the previous asset and
  /// its selections, geometry and cameras; only error() changes. Success resets variant
  /// and animation selections. Performs blocking IO and allocation; no path is borrowed.
  /// A moved-from adapter may be reused by loading a new asset.
  [[nodiscard]] std::expected<void, std::string> load(std::string_view path);
  [[nodiscard]] bool wears(std::string_view variant);
  [[nodiscard]] const std::string &error() const;

  [[nodiscard]] const Geometry &geometry() const;

  /// Select clips by zero-based import index and evaluate their combined pose at time zero.
  /// An empty span disables animation and restores the authored pose. No indices are retained.
  /// @param animations Borrowed clip indices; conflicting channels are rejected by the importer.
  /// @return Success or an owned diagnostic. Invalid selections preserve the active clips and
  /// snapshot; subsequent conversion failures may invalidate borrowed geometry and camera data.
  /// Rebuilds CPU geometry and materials and may allocate; serialize with all adapter access.
  /// Requires an adapter that has not been moved from. Success clears error().
  [[nodiscard]] std::expected<void, std::string> plays(std::span<const int> animations);
  [[nodiscard]] int animations() const;
  [[nodiscard]] double durationS() const;
  /// Evaluate selected animation clips at an absolute time in seconds, without advancing a clock.
  /// Samples geometry, camera nodes and supported material factors together: base colour,
  /// metalness, roughness and emissive RGB. Times beyond keys clamp to endpoints.
  /// @param seconds Finite, nonnegative time. Invalid time leaves the asset unchanged.
  /// @return Success, or false with error(); conversion errors may invalidate borrowed data.
  /// Rebuilds CPU geometry and materials, with allocation; serialize with all adapter access.
  [[nodiscard]] bool poses(double seconds);

  /// Whether camera zero has an unambiguous, noncollapsed placement in the current pose.
  [[nodiscard]] bool carriesCamera() const;
  /// Borrow camera zero at the current pose; requires carriesCamera(). No allocation.
  [[nodiscard]] const Scenario::Camera &camera() const;
  /// Number of camera definitions, including definitions without a node placement.
  [[nodiscard]] int cameras() const;
  /// Resolve a camera using the current sampled node transforms and its authored projection.
  /// @param index Zero-based camera definition index.
  /// @param out Caller-owned result; unchanged on failure. No references are retained.
  /// @return False for an invalid index, ambiguous/missing placement or collapsed basis.
  /// Walks the ancestor chain and may allocate; serialize with mutation of this adapter.
  [[nodiscard]] bool camera(int index, Scenario::Camera &out) const;

  /// Failure to derive a camera for the requested viewport.
  enum class FrameError {
    InvalidViewport, ///< Width or height is not positive.
    InvalidBounds    ///< The loaded scene has no finite, nonzero frameable bounds.
  };
  /// Derive an owned perspective camera for the transformed scene bounds, in metres.
  /// Uses both viewport axes, a five-percent framing margin and bounds-derived depth planes.
  /// Does not mutate the asset or retain viewport data. Serialize with load/poses/wears.
  /// @param viewport Positive dimensions in physical pixels; only their ratio affects framing.
  /// @return Camera looking at the bounds centre, or a typed viewport/bounds error.
  [[nodiscard]] std::expected<Scenario::Camera, FrameError> frames(Extent viewport) const noexcept;

  [[nodiscard]] bool frames(double fill, Scenario::Camera &out) const;
  [[nodiscard]] bool frames(Scenario::Camera &out) const;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

}

#endif
