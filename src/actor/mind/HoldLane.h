#ifndef OUTSHINE_ACTOR_MIND_HOLDLANE_H
#define OUTSHINE_ACTOR_MIND_HOLDLANE_H

#include "Pilot.h"
#include "Task.h"

namespace outshine::Control {

struct Standing {
  const ReferenceLine *Along = nullptr;
  const Pilot::Reins *With = nullptr;
  const Pilot::Placement *At = nullptr;
  double SpeedMs = 0.0;
  double WantedMs = 0.0;
};

class HoldsLane final : public Task {
public:
  [[nodiscard]] const Pilot::Demand &Asked() const { return Asked_; }
  void Sees(const Standing &now) { Now_ = now; }

private:
  [[nodiscard]] Doing Act(double dtS) override;

  Standing Now_;
  Pilot::Demand Asked_;
};

}

#endif
