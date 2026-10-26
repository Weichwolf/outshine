#ifndef OUTSHINE_RENDER_VIEW_VIEWHISTORY_H
#define OUTSHINE_RENDER_VIEW_VIEWHISTORY_H

#include "FrameContext.h"
#include "Mat4.h"
#include "Vec3.h"
#include "TemporalSequence.h"
#include <optional>

namespace outshine::Render {
class ViewHistory {
  struct View {
    Vec3 Translation;
    Mat4f Clip;
  };

public:
  ViewHistory() = default;
  ViewHistory(const ViewHistory &) = delete;
  ViewHistory &operator=(const ViewHistory &) = delete;
  ViewHistory(ViewHistory &&) noexcept = default;
  ViewHistory &operator=(ViewHistory &&) noexcept = default;
  ~ViewHistory() = default;

  class Frame {
  public:
    Frame(ViewHistory &history, bool temporal) noexcept;
    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;
    Frame(Frame &&) = delete;
    Frame &operator=(Frame &&) = delete;
    ~Frame() = default;

    void Commit(const FrameContext &submitted) noexcept;

  private:
    ViewHistory &History_;
    TemporalSequence::Frame Sample_;
  };

  [[nodiscard]] Frame BeginFrame(bool temporal) noexcept { return {*this, temporal}; }

  void ApplyPrevious(FrameContext &frame) const noexcept;
  void ResetAccumulation() noexcept;
  void InvalidateView() noexcept;

  [[nodiscard]] const TemporalSequence &Sequence() const noexcept { return Sequence_; }

  [[nodiscard]] bool HasSubmittedFrame() const noexcept { return Previous_.has_value(); }

private:
  TemporalSequence Sequence_;
  std::optional<View> Previous_;
};
}
#endif
