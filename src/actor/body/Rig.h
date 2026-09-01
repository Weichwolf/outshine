#ifndef OUTSHINE_ACTOR_BODY_RIG_H
#define OUTSHINE_ACTOR_BODY_RIG_H

#include <cstddef>

#include "Vec3.h"
#include "Rigid.h"
#include "Prismatic.h"
#include "Shear.h"

namespace outshine::Physics {

inline constexpr size_t kMaxMounts = 16;

struct Lever {
  double Ratio = 0.0;
};

struct Freedom {
  bool Motion = false;
  Lever Applied;
  Lever Resisting;
};

struct Mount {
  Vec3 AtM;
  Prismatic Strut;
  Shearing Sheds;
  Freedom Steering{.Motion = true, .Applied = {}, .Resisting = {}};
  Freedom Spin{.Motion = false, .Applied = {}, .Resisting = {}};
};

struct Footing {
  bool Found = false;
  double HeightM = 0.0;
  Vec3 NormalM = {{0.0, 1.0, 0.0}};
  double Friction = 1.0;
};

struct Controls {
  double MotionRad = 0.0;
  double AppliedN = 0.0;
  double ResistingN = 0.0;
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
  size_t OffTheSurface = 0;
  double HeaviestN = 0.0;
  double WorstSlipRad = 0.0;
  double WorstRatio = 0.0;
  bool PastTravel = false;
  bool PastLimit = false;
  bool Sliding = false;
};

[[nodiscard]] Reading Bear(Rig &of,
                           const Rigid &body,
                           const Footing *under,
                           const Controls &with,
                           Wrench &into,
                           double dtS);

void Resist(Wrench &into, const Rigid &body, double dragArea, double mediumDensity);

} // namespace outshine::Physics

#endif
