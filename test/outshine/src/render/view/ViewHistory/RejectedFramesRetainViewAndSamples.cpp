#include "ViewHistory.h"
#include "FrameContext.h"
#include "Mat4.h"
#include "Vec2.h"
#include "Check.h"
#include <array>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

struct ViewParameters {
  double Translation;
  float Scale;
};

FrameContext Current(ViewParameters parameters) {
  const auto [translation, scale] = parameters;
  FrameContext frame;
  frame.PreViewTranslation = {{translation, translation * 2, -translation}};
  frame.Mvp = Mat4f{{scale, 0, 0, 0, 0, scale, 0, 0, 0, 0, 1, 0, 0.5f, 0, 0, 1}};
  return frame;
}

void CheckPrevious(const ViewHistory &history,
                   const FrameContext &current,
                   const FrameContext &expected) {
  auto frame = current;
  history.ApplyPrevious(frame);
  CHECK(frame.PrevPreViewTranslation == expected.PreViewTranslation &&
            frame.PrevMvp == expected.Mvp,
        "reprojection uses the complete last accepted camera, with double precision translation");
  CHECK(frame.PreViewTranslation == current.PreViewTranslation && frame.Mvp == current.Mvp &&
            frame.ViewPosition == current.ViewPosition,
        "applying previous camera data preserves the current view");
}
}

int main() {
  ViewHistory history;
  constexpr double kLargeTranslationM = -4000000.125;
  const auto first = Current({.Translation = kLargeTranslationM, .Scale = 2});
  const auto second = Current({.Translation = kLargeTranslationM + 0.5, .Scale = 3});
  const auto third = Current({.Translation = kLargeTranslationM + 1, .Scale = 4});
  CheckPrevious(history, first, first);
  Vec2f firstSample;
  {
    const auto frame = history.BeginFrame(true);
    firstSample = history.Sequence().Jitter();
    CHECK(!history.HasSubmittedFrame() && history.Sequence().OutputIndex() == 1,
          "recording a first image does not publish its camera");
  }
  CHECK(!history.HasSubmittedFrame() && history.Sequence().OutputIndex() == 0 &&
            history.Sequence().Jitter() == Vec2f{},
        "an abandoned first frame publishes neither camera nor sample");
  {
    auto frame = history.BeginFrame(true);
    CHECK(history.Sequence().Jitter() == firstSample, "the retry uses the same first sample");
    frame.Commit(first);
  }
  CHECK(history.HasSubmittedFrame(), "an accepted image publishes its camera");
  CheckPrevious(history, second, first);
  Vec2f secondSample;
  {
    const auto frame = history.BeginFrame(true);
    secondSample = history.Sequence().Jitter();
    CheckPrevious(history, second, first);
  }
  CHECK(history.Sequence().OutputIndex() == 1 && history.Sequence().Jitter() == firstSample,
        "a later abandoned image retains the previous sample and output slot");
  CheckPrevious(history, third, first);
  {
    auto frame = history.BeginFrame(true);
    CHECK(history.Sequence().Jitter() == secondSample && history.Sequence().HasHistory(),
          "the repeated next frame sees the same sample and accepted history");
    frame.Commit(second);
  }
  CheckPrevious(history, third, second);
  const auto slot = history.Sequence().OutputIndex();
  {
    auto frame = history.BeginFrame(false);
    frame.Commit(third);
  }
  CHECK(history.Sequence().OutputIndex() == slot && history.Sequence().Jitter() == secondSample,
        "a non-temporal frame preserves the sample sequence");
  CheckPrevious(history, first, third);
  return Report();
}
