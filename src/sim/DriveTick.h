#ifndef OUTSHINE_SIM_DRIVETICK_H
#define OUTSHINE_SIM_DRIVETICK_H

#include <cstddef>
#include <cstdint>

#include <Scenario.h>

#include "Rigid.h"
#include "CorridorLay.h"
#include "Rig.h"
#include "HoldLane.h"
#include "Rigging.h"
#include "Support.h"

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
  double CalmBeforeWorstAtM = 0.0;
  double AimAtCalmM = 0.0;
  double AimAtWorstM = 0.0;
  long OffsetSamples = 0;
  double LeastClearanceM = 0.0;
  double LeastClearanceAtM = 0.0;
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
  double LeftSteerRad = 0.0;
  double LeftKinematicSteerRad = 0.0;
  double LeftFrontSlipRad = 0.0;
  double LeftRearSlipRad = 0.0;
  double LeftAimStillMovingM = 0.0;
  double StrayedAtM = 0.0;
  double StrayedCurvature = 0.0;
  double StrayedRate = 0.0;
  double StrayedAtMs = 0.0;
  double StrayedPlannedMs = 0.0;
  double StrayedOffsetM = 0.0;
  double StrayedHeadingErrorRad = 0.0;
  double LeftHeadingErrorRad = 0.0;
  double LeftWantAsideM = 0.0;
  double LeftRoomM = 0.0;
  double LeftHalfWidthM = 0.0;
  double LeftBankRad = 0.0;
  double LeftSlope = 0.0;
  double SimulatedS = 0.0;
  bool Advanced = false;
  long Stalls = 0;
  double LongestStallS = 0.0;
  double LongestStallAtM = 0.0;
  bool WasTaken = false;
  double MindSteerRad = 0.0;
  long GroundAsked = 0;
  long GroundAnswered = 0;
};

struct DriveState {
  Control::HoldsLane Keeping;
  size_t Kept = 0;
  Physics::Rig Rig;
  Physics::Rigid Body;
  double CarWidthM = 0.0;
  double NearM = 0.0;
  double LostM = 0.0;
  bool HaveAside = false;
  double HeldAsideM = 0.0;
  double AsideRatePerM = 0.0;
  double CalmAtM = 0.0;
  static constexpr size_t kOffsetBins = 512;
  static constexpr double kOffsetBinM = 0.005;
  uint32_t OffsetBin[kOffsetBins] = {0};
  uint32_t ClearBin[kOffsetBins] = {0};
  double LastReachedM = -1.0;
  double StalledForS = 0.0;
  double CalmAimM = 0.0;
  double SimulatedS = 0.0;
  bool Advanced = false;
  long Stalls = 0;
  double LongestStallS = 0.0;
  double LongestStallAtM = 0.0;
  Ridden Tally;
};

[[nodiscard]] const Ridden &DriveTick(const Corridor &way,
                                      const Rigged &stood,
                                      const Support &beneath,
                                      DriveState &drive,
                                      double dtS,
                                      const Taken *taken);

}

#endif
