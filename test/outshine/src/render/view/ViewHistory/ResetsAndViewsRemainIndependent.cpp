#include "ViewHistory.h"
#include "FrameContext.h"
#include "Mat4.h"
#include "Vec2.h"
#include "Check.h"
#include <array>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

void Submit(ViewHistory &history, const FrameContext &camera) {
  auto frame = history.BeginFrame(true);
  frame.Commit(camera);
}
}

int main() {
  ViewHistory first;
  ViewHistory second;
  FrameContext camera;
  camera.PreViewTranslation = {{1, 2, 3}};
  camera.Mvp = Mat4f{{1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 4, 0, 0, 1}};
  Submit(first, camera);
  CHECK(!second.HasSubmittedFrame() && second.Sequence().Jitter() == Vec2f{},
        "submitting one view leaves a new independent view empty");
  FrameContext other = camera;
  other.PreViewTranslation = {{4, 5, 6}};
  other.Mvp[0] = 4;
  Submit(second, other);
  Submit(second, other);
  const auto secondSample = second.Sequence().Jitter();
  const auto secondSlot = second.Sequence().OutputIndex();
  first.ResetAccumulation();
  CHECK(first.HasSubmittedFrame() && first.Sequence().Jitter() == Vec2f{} &&
            first.Sequence().OutputIndex() == 0 && !first.Sequence().HasHistory(),
        "an accumulation reset retains the accepted camera but forgets temporal samples");
  auto resolved = other;
  first.ApplyPrevious(resolved);
  CHECK(resolved.PrevPreViewTranslation == camera.PreViewTranslation &&
            resolved.PrevMvp == camera.Mvp,
        "a measurement reset retains the previous view required by velocity");
  CHECK(second.Sequence().Jitter() == secondSample &&
            second.Sequence().OutputIndex() == secondSlot && second.Sequence().HasHistory(),
        "resetting one view leaves another view's temporal state intact");
  second.InvalidateView();
  CHECK(!second.HasSubmittedFrame() && second.Sequence().Jitter() == secondSample &&
            second.Sequence().OutputIndex() == secondSlot,
        "target invalidation forgets the available camera without advancing or resetting samples");
  second.ApplyPrevious(resolved);
  CHECK(resolved.PrevPreViewTranslation == resolved.PreViewTranslation &&
            resolved.PrevMvp == resolved.Mvp,
        "an invalidated target starts velocity from its current camera");
  ViewHistory published = std::move(first);
  published.ApplyPrevious(resolved);
  CHECK(published.HasSubmittedFrame() &&
            resolved.PrevPreViewTranslation == camera.PreViewTranslation &&
            resolved.PrevMvp == camera.Mvp && published.Sequence().OutputIndex() == 0,
        "moving a prepared view preserves its accepted camera and reset sample state");
  {
    const auto frame = published.BeginFrame(true);
    CHECK(!published.Sequence().HasHistory() && published.Sequence().OutputIndex() == 1,
          "a reset view starts the original first temporal output slot");
  }
  CHECK(published.Sequence().OutputIndex() == 0,
        "abandoning the first post-reset frame restores that view's reset state");
  return Report();
}
