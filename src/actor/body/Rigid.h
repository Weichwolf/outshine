#ifndef OUTSHINE_ACTOR_BODY_RIGID_H
#define OUTSHINE_ACTOR_BODY_RIGID_H

#include <array>
#include <span>

#include "math/Quat.h"
#include "math/Vec3.h"

namespace outshine::Physics {

struct Rigid {
  double MassKg = 0.0;
  alignas(16) Vec3 InertiaKgM2;
  alignas(16) Vec3 PositionM;
  alignas(16) Quat OrientationQ = {.X = 0.0, .Y = 0.0, .Z = 0.0, .W = 1.0};
  alignas(16) Vec3 VelocityMs;
  alignas(16) Vec3 SpinBodyRadS;
};

static_assert(alignof(Rigid) == 16 && sizeof(Rigid) == 176,
              "each vector row of the body starts on a 128-bit boundary; the 40 bytes over the "
              "packed 136 are the declared price of whole-row NEON loads");

struct Wrench {
  alignas(16) Vec3 ForceN;
  alignas(16) Vec3 TorqueNm;
};

static_assert(alignof(Wrench) == 16 && sizeof(Wrench) == 64,
              "force and torque rows start on 128-bit boundaries; 16 bytes over the packed 48");

void Turn(const Quat &orientationQ, const Vec3 &bodyV, Vec3 &worldV);
void Unturn(const Quat &orientationQ, const Vec3 &worldV, Vec3 &bodyV);

void Place(const Rigid &body, const Vec3 &atBodyM, Vec3 &worldM);

void Lie(Rigid &body, const Vec3 &aheadM, const Vec3 &upM);
void Carry(const Rigid &body, const Vec3 &atBodyM, Vec3 &worldMs);

void Push(Wrench &wrench, const Rigid &body, const Vec3 &atBodyM, const Vec3 &forceN);
void Fall(Wrench &wrench, const Rigid &body, const Vec3 &gravityMs2);

void Step(Rigid &body, const Wrench &wrench, double dtS);

[[nodiscard]] double EnergyJ(const Rigid &body, const Vec3 &gravityMs2);

} // namespace outshine::Physics

#endif
