#ifndef OUTSHINE_SIM_DRIVETICK_H
#define OUTSHINE_SIM_DRIVETICK_H

#include <cstddef>

#include <outshine/Scenario.h>

#include "Body.h"
#include "CorridorLay.h"
#include "Rig.h"
#include "Rigging.h"

namespace outshine::Sim {

struct Taken {
  bool Has = false;
  double SteerRad = 0.0;
  double Throttle = 0.0;
  double Brake = 0.0;
};

struct Ridden {
  bool Found = false;
  bool Arrived = false;
  bool Lost = false;
  bool PastTravel = false;
  bool PastLimit = false;
  bool OffTheRoad = false;
  double SpeedMs = 0.0;
  double ReachedM = 0.0;
  double TopMs = 0.0;
  double WorstOffsetM = 0.0;
  double WorstOffsetAtM = 0.0;
  double WorstRatio = 0.0;
  double WorstRatioAtM = 0.0;
  bool Slid = false;
  double SlidFirstAtM = 0.0;
  double SlidM = 0.0;
  size_t MostAirborne = 0;
  double AirborneAtM = 0.0;
  double BrokeAtM = 0.0;
  double LeftTheRoadAtM = 0.0;
  double LeftByM = 0.0;
  double LeftAtMs = 0.0;
  double LeftPlannedMs = 0.0;
  double LeftCurvature = 0.0;
  double LeftRate = 0.0;
  double LeftLaneM = 0.0;
  double LeftEdgeM = 0.0;
  double LeftAsideM = 0.0;
  double LeftAcrossM = 0.0;
  double SimulatedS = 0.0;
  bool WasTaken = false;
  double MindSteerRad = 0.0;
};

struct DriveState {
  Physics::Rig Rig;
  Physics::Body Body;
  double CarWidthM = 0.0;
  double NearM = 0.0;
  double LostM = 0.0;
  bool HaveAside = false;
  double HeldAsideM = 0.0;
  double AsideRatePerM = 0.0;
  double SimulatedS = 0.0;
  Ridden Tally;
};

[[nodiscard]] Ridden DriveTick(const Corridor &way, const Rigged &stood,
                               DriveState &drive, double dtS, const Taken *taken);

}

#endif
