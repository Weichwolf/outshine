#ifndef OUTSHINE_SIM_CORRIDORLAY_H
#define OUTSHINE_SIM_CORRIDORLAY_H

#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <scenario/Scenario.h>

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
  double Friction = 0.0;
};

static_assert(sizeof(Station) == 32, "a station is four doubles and nothing beside them");
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
  double FrameLat = 0.0, FrameLon = 0.0, PerLatM = 1.0, PerLonM = 1.0, FrameAltM = 0.0;
  double AsideFriction = 0.0;
  Laying Made;

  void Bake(double lengthM) {
    Fine.assign(lengthM > 0.0 ? static_cast<size_t>(lengthM / kFineM) + 2u : 1u, Station{});
  }

  [[nodiscard]] bool Laid() const noexcept { return !Fine.empty(); }

  [[nodiscard]] const Station &At(double alongM) const noexcept {
    const size_t fine = alongM > 0.0 ? static_cast<size_t>(alongM / kFineM) : 0u;
    return Fine[fine < Fine.size() ? fine : Fine.size() - 1];
  }
};

inline constexpr double kLagMargin = 4.0;

[[nodiscard]] constexpr std::expected<double, std::string_view>
AsideRatePerM(double budgetM, double heldMs) noexcept {
  if (!(heldMs > 0.0)) {
    return std::unexpected("a lateral rate is scaled to a speed the plan HOLDS, and this one "
                           "holds none");
  }
  if (!(heldMs < std::numeric_limits<double>::infinity())) {
    return std::unexpected("a lateral rate is scaled to a speed the plan HOLDS, and an "
                           "unbounded one -- a declared vacuum, where drag allows any speed at "
                           "all -- is not a speed anything holds");
  }
  return budgetM / (kLagMargin * Pilot::kSettleS * heldMs);
}

static_assert(!AsideRatePerM(1.0, 0.0).has_value(),
              "a rate scaled to no speed at all is refused, and the compiler says so");
static_assert(!AsideRatePerM(1.0, -3.0).has_value(),
              "a rate scaled to a speed running backwards is refused");
static_assert(!AsideRatePerM(1.0, std::numeric_limits<double>::infinity()).has_value(),
              "a rate scaled to an unbounded speed -- a declared vacuum -- is refused");
static_assert(AsideRatePerM(0.7195, 30.0).has_value(),
              "and a finite positive speed carries a rate");
static_assert(AsideRatePerM(0.7195, 30.0).value() > 0.0, "which is positive");

[[nodiscard]] bool LayCorridor(const Path::Route &route,
                               const GroundQuery &ground,
                               const Body &car,
                               const Rigged &stood,
                               double quantumM,
                               double tightestM,
                               double middleLat,
                               double sphereRadiusM,
                               Sink &say,
                               Corridor &out,
                               std::string &error);

} // namespace outshine::Sim

#endif
