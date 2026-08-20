#ifndef OUTSHINE_PHYSICS_RIG_H
#define OUTSHINE_PHYSICS_RIG_H

#include <cstddef>

#include "Body.h"
#include "Contact.h"
#include "Shear.h"

namespace outshine::Physics {

inline constexpr size_t kMaxMounts = 16;

struct Mount {
  double AtM[3] = {0.0, 0.0, 0.0};
  Contact Touches;
  Slip Sheds;
  double SteeredShare = 0.0;
  double DrivenShare = 0.0;
  double BrakedShare = 0.0;
};

struct Footing {
  bool Found = false;
  double HeightM = 0.0;
  double NormalM[3] = {0.0, 1.0, 0.0};
};

struct Controls {
  double SteerRad = 0.0;
  double DriveN = 0.0;
  double BrakeN = 0.0;
};

struct Rig {
  Mount Mounts[kMaxMounts];
  size_t Count = 0;
  double HeldSlipRad[kMaxMounts] = {0.0};
};

struct Reading {
  size_t Count = 0;
  bool Touching[kMaxMounts] = {false};
  double LoadN[kMaxMounts] = {0.0};
  double PressedM[kMaxMounts] = {0.0};
  double SlipRad[kMaxMounts] = {0.0};
  double RatioOfHold[kMaxMounts] = {0.0};
  size_t Airborne = 0;
  double HeaviestN = 0.0;
  double WorstSlipRad = 0.0;
  double WorstRatio = 0.0;
  bool PastTravel = false;
  bool PastLimit = false;
  bool Sliding = false;
};

Reading Bear(Rig &of, const Body &body, const Footing *under, const Controls &with, Wrench &into,
             double dtS);

void Resist(Wrench &into, const Body &body, double dragArea, double mediumDensity);

} // namespace outshine::Physics

#endif
