#ifndef OUTSHINE_GLTF_IMPORTER_H
#define OUTSHINE_GLTF_IMPORTER_H

#include <memory>
#include <expected>
#include "Extent.h"
#include <span>
#include <string>
#include <string_view>

#include "scene/Geometry.h"
#include "scenario/Scenario.h"

namespace outshine {

/// Owning glTF/GLB import adapter exposing native geometry and cameras.
/// Loading and pose evaluation may allocate and perform substantial CPU work; keep them
/// outside the frame hot path. Serialize all access, including reads of borrowed data.
/// Borrowed geometry/camera data expires on successful mutation, move or destruction.
/// No platform-thread affinity; callers must serialize access. Except load(), frameCamera(Extent),
/// assignment and destruction, members require an object that has not been moved from.
class GltfImporter {
public:
  /// Create an empty adapter with owned storage; may allocate. No filesystem or GPU access.
  GltfImporter();
  /// Release imported data and invalidate all borrowed views; no GPU resources are owned.
  ~GltfImporter();
  /// Transfer ownership without allocation; borrowed views must be reacquired from the destination.
  /// @param other Source left empty; reusable through load() or assignment.
  GltfImporter(GltfImporter &&other) noexcept;
  /// Release previous data and transfer ownership without allocation; invalidates borrowed views.
  /// @param other Source left empty; self-move is valid but leaves an unspecified state.
  /// @return This adapter.
  GltfImporter &operator=(GltfImporter &&other) noexcept;
  /// Copying an owning import adapter is forbidden.
  GltfImporter(const GltfImporter &) = delete;
  /// Copy assignment is forbidden; explicitly load another adapter instead.
  GltfImporter &operator=(const GltfImporter &) = delete;

  /// Load and convert a complete asset before replacing the current one.
  /// @param path Borrowed filesystem path; external resources resolve relative to the asset.
  /// @return Success, or an owned diagnostic. Failure preserves the previous asset and
  /// its selections, geometry and cameras; only error() changes. Success resets variant
  /// and animation selections. Performs blocking IO and allocation; no path is borrowed.
  /// A moved-from adapter may be reused by loading a new asset.
  [[nodiscard]] std::expected<void, std::string> load(std::string_view path);
  /// Select a named material variant and rebuild the currently selected clips at time zero.
  /// @param variant Exact, case-sensitive imported name; copied, never retained as a view.
  /// @return Success or an owned diagnostic for an unknown name or conversion failure. Unknown
  /// names leave the selection and snapshot unchanged; conversion failure may leave partially
  /// rebuilt data. May allocate and decode textures. Success clears error().
  [[nodiscard]] std::expected<void, std::string> selectMaterialVariant(std::string_view variant);
  /// Read the last recorded diagnostic without allocation; not an independent success indicator.
  /// @return Borrowed diagnostic; copy if needed beyond the next mutation, move or destruction.
  /// Successful load()/selectMaterialVariant()/selectAnimations()/sampleAnimation() clears it;
  /// queries leave it unchanged.
  [[nodiscard]] const std::string &error() const;

  /// Read the latest native snapshot without copying or allocating; empty before first load.
  /// @return Borrowed geometry in native metres, right-handed Y-up coordinates.
  /// Mutation can replace its backing buffers; retain an owned copy for independent lifetime.
  [[nodiscard]] const Geometry &geometry() const;

  /// Select clips by zero-based import index and evaluate their combined pose at time zero.
  /// An empty span disables animation and restores the authored pose. No indices are retained.
  /// @param animations Borrowed clip indices; conflicting channels are rejected by the importer.
  /// @return Success or an owned diagnostic. Invalid selections preserve the active clips and
  /// snapshot; subsequent conversion failures may invalidate borrowed geometry and camera data.
  /// Rebuilds CPU geometry and materials and may allocate; serialize with all adapter access.
  /// Requires an adapter that has not been moved from. Success clears error().
  [[nodiscard]] std::expected<void, std::string> selectAnimations(std::span<const int> animations);
  /// @return Number of imported clip definitions, independent of the active selection.
  /// Constant-time, no allocation; zero for an empty adapter.
  [[nodiscard]] int animationCount() const;
  /// @return Last key time in seconds across active clips, or zero when animation is disabled.
  /// Absolute timeline end, not end minus first key time. Constant-time, no allocation.
  [[nodiscard]] double durationS() const;
  /// Evaluate selected animation clips at an absolute time in seconds, without advancing a clock.
  /// Samples geometry, camera nodes and supported material factors together: base colour,
  /// metalness, roughness and emissive RGB. Times beyond keys clamp to endpoints.
  /// @param seconds Finite, nonnegative time. Invalid time leaves the asset unchanged.
  /// @return Success or an owned diagnostic; conversion errors may invalidate borrowed data.
  /// Success clears error(); rejected time changes only the diagnostic.
  /// Rebuilds CPU geometry and materials, with allocation; serialize with all adapter access.
  [[nodiscard]] std::expected<void, std::string> sampleAnimation(double seconds);

  /// @return Whether camera zero has an unambiguous, noncollapsed placement in the current pose.
  /// Constant-time, no allocation; false for an empty adapter.
  [[nodiscard]] bool hasDefaultCamera() const;
  /// Borrow camera zero at the current pose; requires hasDefaultCamera(). No allocation.
  /// @return Native camera view, invalidated by mutation, move or destruction.
  [[nodiscard]] const Scenario::Camera &camera() const;
  /// @return Number of camera definitions, including definitions without a node placement.
  /// Constant-time, no allocation; zero for an empty adapter.
  [[nodiscard]] int cameraCount() const;
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
  /// Does not allocate, mutate the asset or retain viewport data. Serialize with mutation.
  /// @param viewport Positive dimensions in physical pixels; only their ratio affects framing.
  /// @return Camera looking at the bounds centre, or a typed viewport/bounds error.
  [[nodiscard]] std::expected<Scenario::Camera, FrameError>
  frameCamera(Extent viewport) const noexcept;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

}

#endif
