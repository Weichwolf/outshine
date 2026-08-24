#ifndef OUTSHINE_SIM_CORRIDORLAY_H
#define OUTSHINE_SIM_CORRIDORLAY_H

#include <string>
#include <type_traits>
#include <vector>

#include <outshine/Scenario.h>

#include "GroundQuery.h"

#include "Fit.h"
#include "Pilot.h"
#include "ReferenceLine.h"
#include "Rigging.h"
#include "Sink.h"
#include "SpeedProfile.h"
#include "Wayfinding.h"

namespace outshine::Sim {

struct Station {
  double AsideM = 0.0;
  double EdgeM = 0.0;
  double LaneHalfM = 0.0;
};

static_assert(sizeof(Station) == 24, "a station is three doubles and nothing beside them");
static_assert(std::is_trivially_copyable_v<Station>,
              "a station is copied by the byte on the frame path");

struct Laying {
  long Resolved = 0;
  long Holes = 0;
  long LanelessKinds = 0;
  long GradelessKinds = 0;
  double NarrowestHalfM = 0.0;
  double WorstGradeM = 0.0;
  double ClimbLimit = 0.0;
  bool Rose = false;
};

struct Corridor {
  ReferenceLine Line;
  outshine::Fitted Fitted;
  SpeedProfile Profile;
  static constexpr double kFineM = 2.0;

  std::vector<Station> Fine;
  double SpanM = 0.0;
  double NarrowestLaneM = 0.0;
  double BudgetM = 0.0;
  double AsideRatePerM = 0.0;
  double HoldWithinM = 0.0;
  double ReserveMs2 = 0.0;
  double FrameLat = 0.0, FrameLon = 0.0, PerLatM = 1.0, PerLonM = 1.0;
  Laying Made;

  void Bake(double lengthM) {
    Fine.assign(lengthM > 0.0 ? (size_t)(lengthM / kFineM) + 2u : 1u, Station{});
  }

  [[nodiscard]] bool Laid() const noexcept { return !Fine.empty(); }

  [[nodiscard]] const Station &At(double alongM) const noexcept {
    const size_t fine = alongM > 0.0 ? (size_t)(alongM / kFineM) : 0u;
    return Fine[fine < Fine.size() ? fine : Fine.size() - 1];
  }
};

inline constexpr double kLagMargin = 4.0;

[[nodiscard]] constexpr double AsideRatePerM(double budgetM, double topMs) noexcept {
  const double reachM = Pilot::kSettleS * topMs;
  return reachM > 0.0 ? budgetM / (kLagMargin * reachM) : 0.0;
}

[[nodiscard]] bool LayCorridor(const Path::Route &route, const GroundQuery &ground,
                               const Vehicle &car, const Rigged &stood, double quantumM,
                               double tightestM, double middleLat, double sphereRadiusM,
                               Sink &say, Corridor &out, std::string &error);

}

#endif
