#include <cstdio>
#include <string>
#include <vector>

#include <Scenario.h>

#include "Check.h"
#include "Triggers.h"

namespace {

// The oracle is what ENTER means, and it is not a matter of taste: entering is a TRANSITION and
// not a state. A body that stands still inside a volume has entered it once, however many times
// it is asked; a body that leaves and returns has entered it twice. A field that fired on the
// state would fire sixty times a second for a parked car, and a client reading that channel
// would be told the same thing until it stopped listening.
//
// This case exists because the map said this field fires NOTHING, and the map was wrong. It
// fires. What was invisible is that `Engine::Rides` reports a firing into the trace the DRIVER
// prints when assembly ends, and the firing happens on the first `Advance` -- after the print.
// The field was never the defect; the reader was standing at the wrong moment.
constexpr double kExtentM = 100.0;

[[nodiscard]] outshine::Volume Boxed(const char *fires, const char *when) {
  outshine::Volume made;
  made.Id = "box";
  made.Shape = "box";
  made.ExtentM[0] = made.ExtentM[1] = made.ExtentM[2] = kExtentM;
  made.Fires = fires;
  made.When = when;
  return made;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<outshine::Event> events{outshine::Event{"arrived", {}}};

  {
    const std::vector<outshine::Volume> volumes{Boxed("arrived", "enter")};
    auto stood = outshine::TriggerField::Stand(volumes, events);
    CHECK(stood.has_value(), "a box that fires a declared event stands");
    if (!stood) {
      std::printf("REFUSED %s\n", stood.error().c_str());
      return Report();
    }
    outshine::TriggerField field = std::move(*stood);

    const double inside[3] = {1.0, 2.0, 3.0};
    const double outside[3] = {1000.0, 0.0, 0.0};

    field.Probe(0, inside, 0.0);
    const size_t first = field.Drain().size();
    field.Probe(0, inside, 0.1);
    const size_t again = field.Drain().size();
    field.Probe(0, outside, 0.2);
    const size_t leaving = field.Drain().size();
    field.Probe(0, inside, 0.3);
    const size_t returning = field.Drain().size();

    std::printf("ARRIVING fires %zu, STAYING fires %zu, LEAVING fires %zu, RETURNING fires %zu\n",
                first,
                again,
                leaving,
                returning);

    CHECK(first == 1,
          "**A BODY INSIDE A DECLARED VOLUME FIRES ITS EVENT**: the field was said to fire "
          "nothing, and it fires -- what was invisible is that the engine reports a firing into "
          "a trace the driver prints when ASSEMBLY ends, while the firing happens on the first "
          "Advance");
    CHECK(again == 0,
          "and STAYING fires nothing, because entering is a transition and not a state -- a "
          "field that fired on the state would fire sixty times a second for a parked car");
    CHECK(leaving == 0, "and leaving fires nothing, because this volume was declared on enter");
    CHECK(returning == 1,
          "and returning fires once more, because a body that left has entered "
          "again");
  }

  {
    const std::vector<outshine::Volume> volumes{Boxed("arrived", "exit")};
    auto stood = outshine::TriggerField::Stand(volumes, events);
    CHECK(stood.has_value(), "the same box declared on exit stands");
    if (!stood) { return Report(); }
    outshine::TriggerField field = std::move(*stood);

    const double inside[3] = {0.0, 0.0, 0.0};
    const double outside[3] = {1000.0, 0.0, 0.0};
    field.Probe(0, inside, 0.0);
    const size_t arriving = field.Drain().size();
    field.Probe(0, outside, 0.1);
    const size_t leaving = field.Drain().size();

    std::printf("DECLARED ON EXIT: arriving fires %zu, leaving fires %zu\n", arriving, leaving);
    CHECK(arriving == 0 && leaving == 1,
          "and the control is a control: the same box declared on EXIT fires when the body "
          "leaves and not when it arrives, so this case can tell the two edges apart rather "
          "than counting any firing at all");
  }

  Covers("scenario: a declared volume fires on the transition its declaration names -- enter "
         "when a body arrives, exit when it leaves -- and a body that stays fires nothing");
  return Report();
}
