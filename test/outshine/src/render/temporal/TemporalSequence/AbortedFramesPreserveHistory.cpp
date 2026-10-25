#include "TemporalSequence.h"
#include "Check.h"

int main() {
  using outshine::Vec2f;
  using namespace outshine::Render;
  using namespace outshine::Test;
  TemporalSequence sequence;
  CHECK(sequence.OutputIndex() == 0 && !sequence.HasHistory() && sequence.Jitter() == Vec2f{},
        "a new view has no submitted temporal sample");
  Vec2f first;
  {
    const auto frame = sequence.Begin(true);
    first = sequence.Jitter();
    CHECK(sequence.OutputIndex() == 1 && !sequence.HasHistory(),
          "the first frame writes the opposite buffer without reading history");
  }
  CHECK(sequence.OutputIndex() == 0 && !sequence.HasHistory() && sequence.Jitter() == Vec2f{},
        "an unsubmitted first frame does not advance the view");
  {
    auto frame = sequence.Begin(true);
    CHECK(sequence.Jitter() == first, "retry uses the same first sample");
    frame.Commit();
  }
  Vec2f next;
  {
    const auto frame = sequence.Begin(true);
    next = sequence.Jitter();
    CHECK(sequence.OutputIndex() == 0 && sequence.HistoryIndex() == 1 && sequence.HasHistory(),
          "the next frame reads the submitted buffer and writes its partner");
  }
  CHECK(sequence.OutputIndex() == 1 && sequence.Jitter() == first && !sequence.HasHistory(),
        "an abandoned next frame restores all prior temporal state");
  {
    auto frame = sequence.Begin(true);
    CHECK(sequence.Jitter() == next && sequence.HasHistory(),
          "retry keeps the exact next sample and valid previous image");
    const auto delta = sequence.JitterDelta();
    CHECK(delta[0] == next[0] - first[0] && delta[1] == next[1] - first[1],
          "reprojection uses only the previous accepted jitter");
    frame.Commit();
  }
  {
    auto frame = sequence.Begin(false);
    CHECK(sequence.Jitter() == next && sequence.OutputIndex() == 0,
          "non-temporal frames do not advance the sample or output slot");
    frame.Commit();
  }
  sequence.Reset();
  CHECK(sequence.OutputIndex() == 0 && !sequence.HasHistory() && sequence.Jitter() == Vec2f{},
        "a new world or profile clears both sample and history validity");
  {
    auto frame = sequence.Begin(true);
    CHECK(sequence.Jitter() == first && !sequence.HasHistory(),
          "reset restarts the same sequence without stale history");
    frame.Commit();
  }
  return Report();
}
