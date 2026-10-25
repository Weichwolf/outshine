#include "TemporalSequence.h"
#include "Check.h"
#include <array>
#include <cstddef>

int main() {
  using outshine::Vec2f;
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr float kSampleTolerance = 0.0000001f;
  constexpr std::array<Vec2f, 8> expected{{{{-0.25f, 1.0f / 6}},
                                           {{0.25f, -7.0f / 18}},
                                           {{-0.375f, -1.0f / 18}},
                                           {{0.125f, 5.0f / 18}},
                                           {{-0.125f, -5.0f / 18}},
                                           {{0.375f, 1.0f / 18}},
                                           {{-0.4375f, 7.0f / 18}},
                                           {{0.0f, -1.0f / 6}}}};
  TemporalSequence sequence;
  std::array<Vec2f, 8> firstCycle{};
  for (size_t at = 0; at < 3 * expected.size(); ++at) {
    auto frame = sequence.Begin(true);
    const size_t slot = at % expected.size();
    for (size_t axis = 0; axis < 2; ++axis) {
      CHECK_NEAR(sequence.Jitter()[axis],
                 expected[slot][axis],
                 kSampleTolerance,
                 "px",
                 "Halton base-two/base-three samples retain the declared order");
    }
    CHECK(sequence.OutputIndex() == (at + 1) % 2 && sequence.HasHistory() == (at != 0),
          "buffer alternation and history validity follow accepted submissions");
    if (at < expected.size()) {
      firstCycle[slot] = sequence.Jitter();
    } else {
      CHECK(sequence.Jitter() == firstCycle[slot], "subsequent cycles repeat exactly");
    }
    frame.Commit();
  }
  return Report();
}
