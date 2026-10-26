#ifndef OUTSHINE_RENDER_FRAME_FRAMETIMELINE_H
#define OUTSHINE_RENDER_FRAME_FRAMETIMELINE_H

#include "Device.h"
#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace outshine::Render {

class FrameTimeline {
public:
  static constexpr size_t Capacity = 2;

  FrameTimeline() = default;
  FrameTimeline(const FrameTimeline &) = delete;
  FrameTimeline &operator=(const FrameTimeline &) = delete;

  [[nodiscard]] const std::optional<Native::Completion> &Next() const noexcept {
    return Pending_[Next_];
  }

  [[nodiscard]] std::span<const std::optional<Native::Completion>> Pending() const noexcept {
    return Pending_;
  }

  [[nodiscard]] Native::Completion LastSubmitted() const noexcept { return Last_; }

  void Submitted(Native::Completion completion) noexcept;
  void Retire(Native::Completion completion) noexcept;
  void RetireAll() noexcept;

private:
  std::array<std::optional<Native::Completion>, Capacity> Pending_{};
  size_t Next_ = 0;
  Native::Completion Last_{};
};

}
#endif
