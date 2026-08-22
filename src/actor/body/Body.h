#ifndef OUTSHINE_ACTOR_BODY_BODY_H
#define OUTSHINE_ACTOR_BODY_BODY_H

namespace outshine::Physics {

struct Body {
  double MassKg = 0.0;
  alignas(16) double InertiaKgM2[3] = {0.0, 0.0, 0.0};
  alignas(16) double PositionM[3] = {0.0, 0.0, 0.0};
  alignas(16) double OrientationQ[4] = {1.0, 0.0, 0.0, 0.0};
  alignas(16) double VelocityMs[3] = {0.0, 0.0, 0.0};
  alignas(16) double SpinBodyRadS[3] = {0.0, 0.0, 0.0};
};
static_assert(alignof(Body) == 16 && sizeof(Body) == 176,
              "each vector row of the body starts on a 128-bit boundary; the 40 bytes over the "
              "packed 136 are the declared price of whole-row NEON loads");

struct Wrench {
  alignas(16) double ForceN[3] = {0.0, 0.0, 0.0};
  alignas(16) double TorqueNm[3] = {0.0, 0.0, 0.0};
};
static_assert(alignof(Wrench) == 16 && sizeof(Wrench) == 64,
              "force and torque rows start on 128-bit boundaries; 16 bytes over the packed 48");

void Turn(const double orientationQ[4], const double bodyV[3], double worldV[3]);
void Unturn(const double orientationQ[4], const double worldV[3], double bodyV[3]);

void Place(const Body &body, const double atBodyM[3], double worldM[3]);

void Lie(Body &body, const double aheadM[3], const double upM[3]);
void Carry(const Body &body, const double atBodyM[3], double worldMs[3]);

void Push(Wrench &wrench, const Body &body, const double atBodyM[3], const double forceN[3]);
void Fall(Wrench &wrench, const Body &body, const double gravityMs2[3]);

void Step(Body &body, const Wrench &wrench, double dtS);

[[nodiscard]] double EnergyJ(const Body &body, const double gravityMs2[3]);

} // namespace outshine::Physics

#endif
