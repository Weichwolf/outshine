#include "FrameTimeline.h"
#include "Check.h"
#include <algorithm>
#include <optional>

namespace {
bool Matches(const std::optional<outshine::Render::Native::Completion> &value,
             outshine::Render::Native::Completion expected) {
  return value && value->Device == expected.Device && value->Serial == expected.Serial;
}
}

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr Native::Completion first{.Device = 1, .Serial = 7};
  constexpr Native::Completion second{.Device = 1, .Serial = 8};
  constexpr Native::Completion third{.Device = 1, .Serial = 9};
  constexpr Native::Completion fourth{.Device = 2, .Serial = 1};
  FrameTimeline frames;
  CHECK(FrameTimeline::Capacity == 2 && frames.Pending().size() == 2 && !frames.Next() &&
            frames.LastSubmitted().Device == 0 && frames.LastSubmitted().Serial == 0,
        "an unused frame owner has two empty slots and no accepted submission");
  frames.Submitted(first);
  CHECK(!frames.Next() && Matches(frames.Pending().front(), first) &&
            frames.LastSubmitted().Serial == first.Serial,
        "the first accepted frame occupies its slot and selects the other slot");
  frames.Submitted(second);
  CHECK(frames.Next() && frames.Next()->Serial == first.Serial &&
            frames.LastSubmitted().Serial == second.Serial,
        "two accepted frames wrap to the still-pending first slot");
  frames.Retire({.Device = 2, .Serial = first.Serial});
  frames.Retire({.Device = first.Device, .Serial = third.Serial});
  CHECK(frames.Next() && frames.Next()->Serial == first.Serial &&
            std::ranges::all_of(frames.Pending(), [](const auto &one) { return one.has_value(); }),
        "a foreign device or unknown serial cannot release a pending frame");
  frames.Retire(second);
  CHECK(frames.Next() && frames.Next()->Serial == first.Serial && !frames.Pending().back() &&
            frames.LastSubmitted().Serial == second.Serial,
        "out-of-order completion releases only its own slot and preserves the measurement bound");
  frames.Retire(first);
  frames.Submitted(third);
  CHECK(!frames.Next() && Matches(frames.Pending().front(), third) &&
            frames.LastSubmitted().Serial == third.Serial,
        "a retired slot is reused without moving an unrelated pending frame");
  frames.RetireAll();
  CHECK(
      !frames.Next() &&
          std::ranges::none_of(frames.Pending(), [](const auto &one) { return one.has_value(); }) &&
          frames.LastSubmitted().Device == third.Device &&
          frames.LastSubmitted().Serial == third.Serial,
      "full GPU settlement releases slots while retaining the last accepted frame");
  frames.Submitted(fourth);
  CHECK(Matches(frames.Pending().back(), fourth) &&
            frames.LastSubmitted().Device == fourth.Device &&
            frames.LastSubmitted().Serial == fourth.Serial && !frames.Next(),
        "settlement preserves ring position and a later device owns its own serial domain");
  frames.Retire(third);
  CHECK(Matches(frames.Pending().back(), fourth),
        "a late prior-domain completion cannot release the new domain's frame");
  frames.Retire(fourth);
  CHECK(std::ranges::none_of(frames.Pending(), [](const auto &one) { return one.has_value(); }),
        "retiring the final matching frame leaves no pending GPU work");
  return Report();
}
