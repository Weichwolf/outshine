#ifndef OUTSHINE_RENDER_CAMERASTATE_H
#define OUTSHINE_RENDER_CAMERASTATE_H

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <optional>

#include "Viewing.h"

namespace outshine::Render {

struct SubjectView {
  Viewpoint Eye;
  bool HasExplicitCamera = false;
  size_t FramedParts = 0;
};

class CameraState {
public:
  void Override(const Viewpoint &viewpoint) noexcept {
    Override_ = viewpoint;
    Binding_ = BindingState::Dirty;
  }

  void FrameSubject() noexcept {
    Override_.reset();
    Binding_ = BindingState::Dirty;
  }

  [[nodiscard]] bool HasOverride() const noexcept { return Override_.has_value(); }

  [[nodiscard]] const Viewpoint &Override() const noexcept {
    assert(Override_.has_value());
    return *Override_;
  }

  [[nodiscard]] SubjectView &Prepared() noexcept { return Prepared_; }

  [[nodiscard]] const SubjectView &Prepared() const noexcept { return Prepared_; }

  void Prepare(Viewpoint viewpoint, bool explicitCamera, size_t framedParts) noexcept {
    Prepared_ = {.Eye = viewpoint, .HasExplicitCamera = explicitCamera, .FramedParts = framedParts};
  }

  [[nodiscard]] bool NeedsBinding() const noexcept { return Binding_ != BindingState::Bound; }

  [[nodiscard]] bool IsUnbound() const noexcept { return Binding_ == BindingState::Unbound; }

  void MarkBound() noexcept { Binding_ = BindingState::Bound; }

  void Unbind() noexcept {
    if (Binding_ == BindingState::Bound) { Binding_ = BindingState::Unbound; }
  }

  void Invalidate() noexcept { Binding_ = BindingState::Dirty; }

  [[nodiscard]] double OrbitDegrees() const noexcept { return OrbitDegrees_; }

  void AdvanceOrbit(double degrees) noexcept { OrbitDegrees_ += degrees; }

private:
  enum class BindingState : uint8_t { Unbound, Bound, Dirty };

  std::optional<Viewpoint> Override_;
  SubjectView Prepared_;
  BindingState Binding_ = BindingState::Unbound;
  double OrbitDegrees_ = 0.0;
};

}
#endif
