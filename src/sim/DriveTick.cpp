#include "DriveTick.h"

#include <cmath>

#include "Contact.h"
#include "Carriageway.h"
#include "Pilot.h"
#include "Drive.h"
#include "Sink.h"
#include "SpeedProfile.h"

namespace outshine::Sim {

namespace {
constexpr double kResectM = 4.0;
constexpr double kFromM = 50.0;

double HeadingOf(const outshine::Physics::Body &body) {
  const double aheadBody[3] = {0.0, 0.0, -1.0};
  double ahead[3];
  outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
  return std::atan2(-ahead[2], ahead[0]);
}
} // namespace

Ridden DriveTick(const Corridor &way, const Rigged &stood,
                 DriveState &drive, double dtS, const Taken *taken) {
  Ridden &out = drive.Tally;
  out.Found = false;
  auto &corridor = way.Line;
  auto &profile = way.Profile;


  auto &rig = drive.Rig;
  auto &body = drive.Body;
  auto &fineAside = way.FineAside;
  auto &fineEdge = way.FineEdge;
  auto &laneHalfM = way.LaneHalfM;
  const double fineM = way.FineM;
  const double spanM = way.SpanM;
  outshine::Pilot::Reins reins;
  reins.SettleS = 1.0;
  reins.LeastReachM = stood.Axles.WheelbaseM;
  reins.HoldWithinM = way.HoldWithinM;
  const double gravity[3] = {0.0, -stood.Envelope.GravityMs2, 0.0};
  out.Found = true;

  const double eastM = body.PositionM[0];
  const double northM = -body.PositionM[2];
  const double headingRad = HeadingOf(body);
  const double windowM = kResectM + 3.0 * drive.LostM;
  const outshine::Pilot::Placement at = outshine::Pilot::Locate(
      corridor, eastM, northM, body.PositionM[1], headingRad, drive.NearM, windowM);
  if (!at.Found) {
    out.Lost = true;
    return out;
  }
  drive.NearM = at.AlongM;
  drive.LostM = std::fabs(at.OffsetM);
  out.ReachedM = at.AlongM;

  const double speedMs = std::sqrt(body.VelocityMs[0] * body.VelocityMs[0] +
                                   body.VelocityMs[2] * body.VelocityMs[2]);
  out.SpeedMs = speedMs;
  reins.TightestPerM = outshine::Pilot::TightestPerM(stood.Axles, stood.Envelope, speedMs);
  {
    const size_t fine = (size_t)(at.AlongM / fineM);
    const double wantAsideM = fineAside[fine < fineAside.size() ? fine : fineAside.size() - 1];
    if (!drive.HaveAside) {
      drive.HeldAsideM = wantAsideM;
      drive.HaveAside = true;
    } else {
      const double mayMoveM = drive.AsideRatePerM * speedMs * dtS;
      const double byM = wantAsideM - drive.HeldAsideM;
      drive.HeldAsideM += std::fabs(byM) <= mayMoveM ? byM : (byM > 0.0 ? mayMoveM : -mayMoveM);
    }
    const double roomM = fineEdge[fine < fineEdge.size() ? fine : fineEdge.size() - 1] -
                         0.5 * drive.CarWidthM - way.BudgetM;
    if (roomM > 0.0) {
      if (drive.HeldAsideM > roomM) { drive.HeldAsideM = roomM; }
      if (drive.HeldAsideM < -roomM) { drive.HeldAsideM = -roomM; }
    }
    reins.AsideM = drive.HeldAsideM;
  }
  const double brakingM =
      speedMs * speedMs / (2.0 * (stood.Envelope.BrakeMs2() > 0.0 ? stood.Envelope.BrakeMs2()
                                                                  : 1.0));
  double wantedMs = profile.At(at.AlongM);
  double needMs2 = 0.0;
  for (int look = 1; look <= 12; ++look) {
    const double overM = brakingM * (double)look / 12.0;
    const double atM = std::fmin(at.AlongM + overM, corridor.LengthM());
    const double thereMs = profile.At(atM);
    if (thereMs < speedMs && overM > 0.0) {
      const double askMs2 = (speedMs * speedMs - thereMs * thereMs) / (2.0 * overM);
      if (askMs2 > needMs2) { needMs2 = askMs2; }
    }
    if (thereMs < wantedMs) { wantedMs = thereMs; }
  }
  outshine::Pilot::Demand asked = Hold(corridor, reins, at, speedMs, wantedMs);
  if (needMs2 > 0.0 && -needMs2 < asked.AlongMs2) { asked.AlongMs2 = -needMs2; }
  const outshine::Pilot::Steering command =
      outshine::Pilot::Drive(stood.Axles, stood.Envelope, asked);

  outshine::Physics::Controls controls;
  controls.SteerRad = command.SteerRad;
  controls.DriveN = command.DriveN;
  controls.BrakeN = command.BrakeN;
  out.MindSteerRad = command.SteerRad;
  out.WasTaken = taken != nullptr && taken->Has;
  if (out.WasTaken) {
    controls.SteerRad = taken->SteerRad;
    controls.DriveN = taken->Throttle * stood.Envelope.DriveN;
    controls.BrakeN = taken->Brake * stood.Envelope.BrakeN;
  }

  outshine::Physics::Footing under[outshine::Physics::kMaxMounts];
  for (size_t which = 0; which < rig.Count; ++which) {
    double worldM[3];
    outshine::Physics::Place(body, rig.Mounts[which].AtM, worldM);
    const size_t band = (size_t)(at.AlongM / fineM) < fineEdge.size()
                            ? (size_t)(at.AlongM / fineM)
                            : fineEdge.size() - 1;
    const double edgeM = fineEdge[band];
    const double armEastM = worldM[0] - eastM;
    const double armNorthM = -worldM[2] - northM;
    const double armAlongM = std::cos(headingRad) * armEastM + std::sin(headingRad) * armNorthM;
    const double armAcrossM = -std::sin(headingRad) * armEastM + std::cos(headingRad) * armNorthM;
    const outshine::Standing on =
        outshine::StandAt(corridor, at.AlongM + armAlongM, at.OffsetM + armAcrossM, 0.0);
    under[which].Found = std::fabs(at.OffsetM + armAcrossM) <= edgeM;
    under[which].HeightM = on.HeightM;
    under[which].NormalM[0] = on.NormalM[0];
    under[which].NormalM[1] = on.NormalM[1];
    under[which].NormalM[2] = -on.NormalM[2];
  }

  outshine::Physics::Wrench wrench;
  outshine::Physics::Fall(wrench, body, gravity);
  outshine::Physics::Resist(wrench, body, stood.Envelope.DragArea, stood.Envelope.AirDensity);
  const outshine::Physics::Reading read =
      outshine::Physics::Bear(rig, body, under, controls, wrench, dtS);

  if (at.AlongM >= kFromM) {
    const double inLaneM = at.OffsetM - reins.AsideM;
    if (std::fabs(inLaneM) > std::fabs(out.WorstOffsetM)) {
      out.WorstOffsetM = inLaneM;
      out.WorstOffsetAtM = at.AlongM;
    }
    out.WorstRatio = std::fmax(out.WorstRatio, read.WorstRatio);
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

  if (read.OffTheSurface > 0 && out.LeftTheRoadAtM <= 0.0) {
    out.LeftTheRoadAtM = at.AlongM;
    out.LeftByM = at.OffsetM - reins.AsideM;
    out.LeftAtMs = speedMs;
    out.LeftPlannedMs = profile.At(at.AlongM);
    out.LeftCurvature = at.CurvaturePerM;
    out.LeftRate = at.CurvatureRatePerM;
    const size_t post = (size_t)(at.AlongM / spanM);
    out.LeftLaneM = 2.0 * laneHalfM[post < laneHalfM.size() ? post : laneHalfM.size() - 1];
    const size_t fine = (size_t)(at.AlongM / fineM);
    out.LeftEdgeM = fineEdge[fine < fineEdge.size() ? fine : fineEdge.size() - 1];
    out.LeftAsideM = reins.AsideM;
    out.LeftAcrossM = at.OffsetM;
  }
  if (read.OffTheSurface > 0) {
    out.OffTheRoad = true;
    out.BrokeAtM = at.AlongM;
    out.LeftTheRoadAtM = at.AlongM;
    return out;
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
  if (at.AlongM >= corridor.LengthM() - 20.0) { out.Arrived = true; }
  return out;
}

} // namespace outshine::Sim
