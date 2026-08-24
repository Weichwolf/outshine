#ifndef OUTSHINE_SIM_CORRIDORLAY_H
#define OUTSHINE_SIM_CORRIDORLAY_H

#include <string>
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

struct Corridor {
  ReferenceLine Line;
  outshine::Fitted Fitted;
  SpeedProfile Profile;
  std::vector<double> RoadM, HalfWidthM, LaneHalfM, AsideM;
  std::vector<Station> Fine;
  double FineM = 2.0;
  double SpanM = 0.0;
  double NarrowestLaneM = 0.0;
  double BudgetM = 0.0;
  double AsideRatePerM = 0.0;
  double HoldWithinM = 0.0;
  double ReserveMs2 = 0.0;
  double FrameLat = 0.0, FrameLon = 0.0, PerLatM = 1.0, PerLonM = 1.0;

  void Bake(double lengthM) { Fine.assign((size_t)(lengthM / FineM) + 2u, Station{}); }

  [[nodiscard]] bool Laid() const { return !Fine.empty(); }

  [[nodiscard]] const Station &At(double alongM) const {
    const size_t fine = (size_t)(alongM / FineM);
    return Fine[fine < Fine.size() ? fine : Fine.size() - 1];
  }
};

inline constexpr double kLagMargin = 2.0;

[[nodiscard]] constexpr double AsideRatePerM(double budgetM, double topMs) {
  const double reachM = Pilot::kSettleS * topMs;
  return reachM > 0.0 ? budgetM / (kLagMargin * reachM) : 0.0;
}

[[nodiscard]] bool LayCorridor(const Path::Route &route, const GroundQuery &ground,
                               const Vehicle &car, const Rigged &stood, double quantumM,
                               double tightestM, double middleLat, double sphereRadiusM,
                               Sink &say, Corridor &out, std::string &error);

}

#endif
