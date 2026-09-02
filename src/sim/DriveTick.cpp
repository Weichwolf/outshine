#include "DriveTick.h"
#include "math/Vec3.h"

#include "HoldLane.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <type_traits>

#include <vector>

#include <cmath>

#include "Prismatic.h"
#include "Carriageway.h"
#include "Pilot.h"
#include "Drive.h"
#include "Sink.h"
#include "SpeedProfile.h"

namespace outshine::Sim {

constexpr size_t kRiddenBytes = 456;

static_assert(sizeof(Ridden) == kRiddenBytes, "sizeof(Ridden)");
static_assert(std::is_trivially_copyable_v<Ridden>, "a tick answer is a value");
static_assert(sizeof(DriveState) >= sizeof(Ridden), "the state holds the tally");

namespace {
constexpr double kResectM = 4.0;
constexpr double kFromM = 50.0;

double HeadingOf(const outshine::Physics::Rigid &body) {
  const Vec3 aheadBody = {{0.0, 0.0, -1.0}};
  Vec3 ahead;
  outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
  return std::atan2(-ahead[2], ahead[0]);
}
} // namespace

const Ridden &DriveTick(const Corridor &way,
                        const Rigged &stood,
                        const Support &beneath,
                        DriveState &drive,
                        double dtS,
                        const Taken *taken) {
  Ridden &out = drive.Tally;
  out.Found = false;
  auto &corridor = way.Line;
  auto &profile = way.Profile;

  auto &rig = drive.Rig;
  auto &body = drive.Body;
  if (!way.Laid()) { return out; }
  outshine::Pilot::Holding reins;
  reins.SettleS = outshine::Pilot::kSettleS;
  reins.LeastReachM = stood.Axles.WheelbaseM;
  reins.HoldWithinM = way.HoldWithinM;
  const Vec3 gravity = {{0.0, -stood.Envelope.GravityMs2, 0.0}};
  out.Found = true;

  const double eastM = body.PositionM[0];
  const double northM = -body.PositionM[2];
  const double headingRad = HeadingOf(body);
  const double windowM = kResectM + 3.0 * drive.LostM;
  const outshine::Pilot::Where at = outshine::Pilot::Locate(
      corridor, eastM, northM, body.PositionM[1], headingRad, drive.NearM, windowM);
  if (!at.Found) {
    out.Lost = true;
    return out;
  }
  drive.NearM = at.AlongM;
  drive.LostM = std::fabs(at.OffsetM);
  out.ReachedM = at.AlongM;
  out.Advanced = drive.LastReachedM < 0.0 || at.AlongM > drive.LastReachedM;
  if (out.Advanced) {
    drive.StalledForS = 0.0;
  } else {
    drive.StalledForS += dtS;
    ++out.Stalls;
    if (drive.StalledForS > out.LongestStallS) {
      out.LongestStallS = drive.StalledForS;
      out.LongestStallAtM = at.AlongM;
    }
  }
  drive.LastReachedM = at.AlongM;

  const double speedMs =
      std::sqrt(body.VelocityMs[0] * body.VelocityMs[0] + body.VelocityMs[2] * body.VelocityMs[2]);
  out.SpeedMs = speedMs;
  reins.TightestPerM = outshine::Pilot::TightestPerM(stood.Axles, stood.Envelope, speedMs);
  double aimStillMovingM = 0.0;
  double wantedAsideM = 0.0;
  double roomHereM = 0.0;
  const Station &here = way.At(at.AlongM);
  {
    const double wantAsideM = here.AsideM;
    if (!drive.HaveAside) {
      drive.HeldAsideM = wantAsideM;
      drive.HaveAside = true;
    } else {
      const double mayMoveM = drive.AsideRatePerM * speedMs * dtS;
      const double byM = wantAsideM - drive.HeldAsideM;
      drive.HeldAsideM += std::clamp(byM, -mayMoveM, mayMoveM);
    }
    const double roomM = here.EdgeM - 0.5 * drive.CarWidthM - way.BudgetM;
    if (roomM > 0.0) {
      drive.HeldAsideM = std::min(drive.HeldAsideM, roomM);
      drive.HeldAsideM = std::max(drive.HeldAsideM, -roomM);
    }
    reins.AsideM = drive.HeldAsideM;
    aimStillMovingM = wantAsideM - drive.HeldAsideM;
    wantedAsideM = wantAsideM;
    roomHereM = roomM;
  }
  const double brakingM = speedMs * speedMs / (2.0 * stood.Envelope.BrakeMs2());
  double wantedMs = profile.At(at.AlongM);
  double needMs2 = 0.0;
  constexpr int kBrakeLooks = 12;
  for (int look = 1; look <= kBrakeLooks; ++look) {
    const double overM = brakingM * static_cast<double>(look) / static_cast<double>(kBrakeLooks);
    const double atM = std::fmin(at.AlongM + overM, corridor.LengthM());
    const double thereMs = profile.At(atM);
    if (thereMs < speedMs && overM > 0.0) {
      const double askMs2 = (speedMs * speedMs - thereMs * thereMs) / (2.0 * overM);
      needMs2 = std::max(askMs2, needMs2);
    }
    wantedMs = std::min(thereMs, wantedMs);
  }
  outshine::Control::Sight sees;
  sees.Along = &corridor;
  sees.With = &reins;
  sees.At = &at;
  sees.SpeedMs = speedMs;
  sees.WantedMs = wantedMs;
  drive.Keeping.Sees(sees);
  if (drive.Keeping.Step(dtS) != outshine::Control::Doing::Abandoned) { drive.Kept += 1u; }
  outshine::Pilot::Demand asked = drive.Keeping.Asked();
  if (needMs2 > 0.0 && -needMs2 < asked.AlongMs2) { asked.AlongMs2 = -needMs2; }
  const outshine::Pilot::Steering command =
      outshine::Pilot::Drive(stood.Axles, stood.Envelope, asked);

  outshine::Physics::Controls controls;
  controls.MotionRad = command.SteerRad;
  controls.AppliedN = command.DriveN;
  controls.ResistingN = command.BrakeN;
  out.MindSteerRad = command.SteerRad;
  out.WasTaken = taken != nullptr && taken->Has;
  if (out.WasTaken) {
    controls.MotionRad = taken->SteerRad;
    controls.AppliedN = taken->Throttle * stood.Envelope.DriveN;
    controls.ResistingN = taken->Brake * stood.Envelope.BrakeN;
  }

  std::array<outshine::Physics::Footing, outshine::Physics::kMaxMounts> under{};
  size_t offMade = 0;
  for (size_t which = 0; which < rig.Count; ++which) {
    Vec3 worldM;
    outshine::Physics::Place(body, rig.Mounts[which].AtM, worldM);
    const double edgeM = here.EdgeM;
    const double armEastM = worldM[0] - eastM;
    const double armNorthM = -worldM[2] - northM;
    const double armAlongM = std::cos(headingRad) * armEastM + std::sin(headingRad) * armNorthM;
    const double armAcrossM = -std::sin(headingRad) * armEastM + std::cos(headingRad) * armNorthM;
    const double acrossM = at.OffsetM + armAcrossM;
    const outshine::Astride on = outshine::StandAt(corridor, at.AlongM + armAlongM, acrossM, 0.0);
    const bool onMade = std::fabs(acrossM) <= edgeM;
    offMade += onMade ? 0u : 1u;
    under[which].Found = true;
    under[which].Friction = onMade ? here.Friction : way.AsideFriction;
    under[which].HeightM = on.HeightM;
    under[which].NormalM[0] = on.NormalM[0];
    under[which].NormalM[1] = on.NormalM[1];
    under[which].NormalM[2] = -on.NormalM[2];
    if (!onMade) {
      const double atLat = way.FrameLat - worldM[2] / way.PerLatM;
      const double atLon = way.FrameLon + worldM[0] / way.PerLonM;
      const Underneath ground = beneath.At(atLat, atLon);
      ++out.GroundAsked;
      if (ground.Known) {
        ++out.GroundAnswered;
        under[which].HeightM = ground.HeightAslM;
        under[which].Friction = ground.Friction;
        under[which].NormalM[0] = ground.NormalM[0];
        under[which].NormalM[1] = ground.NormalM[1];
        under[which].NormalM[2] = -ground.NormalM[2];
      }
    }
  }

  outshine::Physics::Wrench wrench;
  outshine::Physics::Fall(wrench, body, gravity);
  outshine::Physics::Resist(wrench, body, stood.Envelope.DragArea, stood.Envelope.AirDensity);
  const outshine::Physics::Reading read =
      outshine::Physics::Bear(rig, body, under.data(), controls, wrench, dtS);

  if (at.AlongM >= kFromM) {
    const double inLaneM = at.OffsetM - reins.AsideM;
    if (std::fabs(inLaneM) < 0.5 * way.BudgetM) {
      drive.CalmAtM = at.AlongM;
      drive.CalmAimM = reins.AsideM;
    }
    if (std::fabs(inLaneM) > std::fabs(out.WorstOffsetM)) {
      out.WorstOffsetM = inLaneM;
      out.WorstOffsetAtM = at.AlongM;
      out.CalmBeforeWorstAtM = drive.CalmAtM;
      out.AimAtCalmM = drive.CalmAimM;
      out.AimAtWorstM = reins.AsideM;
    }
    {
      const auto bin = static_cast<size_t>(std::fabs(inLaneM) / DriveState::kOffsetBinM);
      ++drive.OffsetBin[bin < DriveState::kOffsetBins ? bin : DriveState::kOffsetBins - 1];
      ++out.OffsetSamples;

      const double clearM = here.EdgeM - std::fabs(at.OffsetM) - 0.5 * drive.CarWidthM;
      if (out.OffsetSamples == 1 || clearM < out.LeastClearanceM) {
        out.LeastClearanceM = clearM;
        out.LeastClearanceAtM = at.AlongM;
      }
      const size_t clearBin =
          clearM <= 0.0 ? 0 : static_cast<size_t>(clearM / DriveState::kOffsetBinM) + 1;
      ++drive.ClearBin[clearBin < DriveState::kOffsetBins ? clearBin : DriveState::kOffsetBins - 1];
    }
    if (out.StrayedAtM <= 0.0) {
      const double halfRoomM = here.LaneHalfM - 0.5 * drive.CarWidthM;
      if (halfRoomM > 0.0 && std::fabs(inLaneM) > 0.5 * halfRoomM) {
        out.StrayedAtM = at.AlongM;
        out.StrayedCurvature = at.CurvaturePerM;
        out.StrayedRate = at.CurvatureRatePerM;
        out.StrayedAtMs = speedMs;
        out.StrayedPlannedMs = profile.At(at.AlongM);
        out.StrayedOffsetM = at.OffsetM;
        out.StrayedHeadingErrorRad = at.HeadingErrorRad;
      }
    }
    if (read.WorstRatio > out.WorstRatio) {
      out.WorstRatio = read.WorstRatio;
      out.WorstRatioAtM = at.AlongM;
    }
    if (read.Sliding) {
      if (!out.Slid) {
        out.Slid = true;
        out.SlidFirstAtM = at.AlongM;
      }
      out.SlidM += speedMs * dtS;
    }
    out.TopMs = std::fmax(out.TopMs, speedMs);
    if (read.PastLimit && !out.PastLimit) {
      out.PastLimit = true;
      out.BrokeAtM = at.AlongM;
    }
    out.PastTravel = out.PastTravel || read.PastTravel;
    if (read.Airborne > out.MostAirborne) {
      out.MostAirborne = read.Airborne;
      out.AirborneAtM = at.AlongM;
    }
  }

  if (offMade > 0 && out.LeftTheRoadAtM <= 0.0) {
    out.LeftTheRoadAtM = at.AlongM;
    out.LeftByM = at.OffsetM - reins.AsideM;
    out.LeftAtMs = speedMs;
    out.LeftPlannedMs = profile.At(at.AlongM);
    out.LeftCurvature = at.CurvaturePerM;
    out.LeftRate = at.CurvatureRatePerM;
    out.LeftLaneM = 2.0 * here.LaneHalfM;
    out.LeftEdgeM = here.EdgeM;
    out.LeftAsideM = reins.AsideM;
    out.LeftAcrossM = at.OffsetM;
    out.LeftSteerRad = controls.MotionRad;
    out.LeftKinematicSteerRad = std::atan(stood.Axles.WheelbaseM * at.CurvaturePerM);
    double frontSlip = 0.0;
    double rearSlip = 0.0;
    for (size_t which = 0; which < read.Count && which < 4; ++which) {
      const double slip = std::fabs(read.SlipRad[which]);
      double &into = which < 2 ? frontSlip : rearSlip;
      into = std::max(slip, into);
    }
    out.LeftAimStillMovingM = aimStillMovingM;
    out.LeftWantAsideM = wantedAsideM;
    out.LeftRoomM = roomHereM;
    out.LeftHalfWidthM = here.EdgeM;
    out.LeftHeadingErrorRad = at.HeadingErrorRad;
    out.LeftBankRad = at.BankRad;
    out.LeftSlope = at.SlopeAt;
    out.LeftFrontSlipRad = frontSlip;
    out.LeftRearSlipRad = rearSlip;
  }
  if (offMade > 0) {
    out.OffTheRoad = true;
    out.LeftTheRoadAtM = at.AlongM;
  }
  if (read.PastLimit || read.Airborne == rig.Count) {
    out.BrokeAtM = at.AlongM;
    out.PastLimit = out.PastLimit || read.PastLimit;
    out.MostAirborne = read.Airborne;
    return out;
  }
  outshine::Physics::Step(body, wrench, dtS);
  drive.SimulatedS += dtS;
  out.SimulatedS = drive.SimulatedS;
  constexpr double kArrivedWithinM = 20.0;
  if (at.AlongM >= corridor.LengthM() - kArrivedWithinM) { out.Arrived = true; }
  return out;
}

} // namespace outshine::Sim
