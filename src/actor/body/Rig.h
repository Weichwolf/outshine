#ifndef OUTSHINE_ACTOR_BODY_RIG_H
#define OUTSHINE_ACTOR_BODY_RIG_H

#include <array>
#include <cstddef>

#include "math/Vec3.h"
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
  std::array<Mount, kMaxMounts> Mounts{};
  size_t Count = 0;
  std::array<double, kMaxMounts> HeldSlipRad = {{0.0}};
};

struct Reading {
  size_t Count = 0;
  std::array<bool, kMaxMounts> Touching = {{false}};
  std::array<double, kMaxMounts> LoadN = {{0.0}};
  std::array<double, kMaxMounts> PressedM = {{0.0}};
  std::array<double, kMaxMounts> SlipRad = {{0.0}};
  std::array<double, kMaxMounts> RatioOfHold = {{0.0}};
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
