#ifndef OUTSHINE_ACTOR_BODY_RIGID_H
#define OUTSHINE_ACTOR_BODY_RIGID_H

#include <array>
#include <span>

#include "Vec3.h"

namespace outshine::Physics {

struct Rigid {
  double MassKg = 0.0;
  alignas(16) Vec3 InertiaKgM2;
  alignas(16) Vec3 PositionM;
  alignas(16) std::array<double, 4> OrientationQ = {1.0, 0.0, 0.0, 0.0};
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

void Turn(std::span<const double, 4> orientationQ,
          std::span<const double, 3> bodyV,
          std::span<double, 3> worldV);
void Unturn(std::span<const double, 4> orientationQ,
            std::span<const double, 3> worldV,
            std::span<double, 3> bodyV);

void Place(const Rigid &body, std::span<const double, 3> atBodyM, std::span<double, 3> worldM);

void Lie(Rigid &body, std::span<const double, 3> aheadM, std::span<const double, 3> upM);
void Carry(const Rigid &body, std::span<const double, 3> atBodyM, std::span<double, 3> worldMs);

void Push(Wrench &wrench,
          const Rigid &body,
          std::span<const double, 3> atBodyM,
          std::span<const double, 3> forceN);
void Fall(Wrench &wrench, const Rigid &body, std::span<const double, 3> gravityMs2);

void Step(Rigid &body, const Wrench &wrench, double dtS);

[[nodiscard]] double EnergyJ(const Rigid &body, std::span<const double, 3> gravityMs2);

} // namespace outshine::Physics

#endif
