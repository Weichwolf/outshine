#ifndef OUTSHINE_RENDER_FRAME_FRAMETARGETS_H
#define OUTSHINE_RENDER_FRAME_FRAMETARGETS_H

#include "Device.h"
#include "ExecutionPlan.h"
#include "Extent.h"
#include <array>
#include <cstddef>
#include <memory>

namespace outshine::Render {
class FrameTargets {
public:
  FrameTargets() = default;
  FrameTargets(const FrameTargets &) = delete;
  FrameTargets &operator=(const FrameTargets &) = delete;
  FrameTargets(FrameTargets &&) noexcept = default;
  FrameTargets &operator=(FrameTargets &&) noexcept = default;
  ~FrameTargets() = default;

  // The frame owner retires previous GPU uses before replacing their attachments.
  [[nodiscard]] Native::Result<void>
  Replace(Native::Device &device, Extent extent, const ExecutionPlan &plan);
  [[nodiscard]] Native::Texture *Get(Resource resource, size_t linearIndex = 0) const noexcept;
  [[nodiscard]] static bool Owns(Resource resource) noexcept;
  [[nodiscard]] size_t Count() const noexcept;

private:
  enum class Slot : size_t {
    Hdr,
    Velocity,
    Depth,
    ShadingNormal,
    SurfaceIdentity,
    SurfaceBase,
    SurfaceMetalRough,
    Transmissive,
    Composited,
    Aerial,
    Frame,
    Linear,
    History,
    Count
  };
  [[nodiscard]] static Slot Index(Resource resource) noexcept;
  std::array<std::unique_ptr<Native::Texture>, static_cast<size_t>(Slot::Count)> Images_{};
};
}
#endif
