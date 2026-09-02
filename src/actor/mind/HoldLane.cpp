#include "HoldLane.h"

namespace outshine::Control {

Doing HoldsLane::Act([[maybe_unused]] double dtS) {
  if (Now_.Along == nullptr || Now_.With == nullptr || Now_.At == nullptr) {
    Asked_ = Pilot::Demand{};
    return Doing::Done;
  }
  Asked_ = Pilot::Hold(*Now_.Along, *Now_.With, *Now_.At, Now_.SpeedMs, Now_.WantedMs);
  return Asked_.AtEnd ? Doing::Done : Doing::Running;
}

} // namespace outshine::Control
