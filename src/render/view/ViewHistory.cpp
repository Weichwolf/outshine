#include "ViewHistory.h"
#include "FrameContext.h"

namespace outshine::Render {
ViewHistory::Frame::Frame(ViewHistory &history, bool temporal) noexcept
    : History_(history), Sample_(history.Sequence_.Begin(temporal)) {}

void ViewHistory::Frame::Commit(const FrameContext &submitted) noexcept {
  History_.Previous_ = View{.Translation = submitted.PreViewTranslation, .Clip = submitted.Mvp};
  Sample_.Commit();
}

void ViewHistory::ApplyPrevious(FrameContext &frame) const noexcept {
  frame.PrevPreViewTranslation = Previous_ ? Previous_->Translation : frame.PreViewTranslation;
  frame.PrevMvp = Previous_ ? Previous_->Clip : frame.Mvp;
}

void ViewHistory::ResetAccumulation() noexcept {
  Sequence_.Reset();
}

void ViewHistory::InvalidateView() noexcept {
  Previous_.reset();
}
}
