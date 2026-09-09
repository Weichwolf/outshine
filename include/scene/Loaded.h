#ifndef OUTSHINE_LOADED_H
#define OUTSHINE_LOADED_H

#include <memory>
#include <expected>
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

  [[nodiscard]] bool plays(std::span<const int> animations);
  [[nodiscard]] int animations() const;
  [[nodiscard]] double durationS() const;
  [[nodiscard]] bool poses(double seconds);

  [[nodiscard]] bool carriesCamera() const;
  [[nodiscard]] const Scenario::Camera &camera() const;
  [[nodiscard]] int cameras() const;
  [[nodiscard]] bool camera(int index, Scenario::Camera &out) const;

  [[nodiscard]] bool frames(double fill, Scenario::Camera &out) const;
  [[nodiscard]] bool frames(Scenario::Camera &out) const;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

}

#endif
