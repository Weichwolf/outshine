#include <array>
#include <span>

#include "math/Quat.h"
#include "Rigid.h"

#include <cmath>

#include "math/Vec3.h"

namespace outshine::Physics {

using outshine::Cross;

constexpr double kQuarterOfRoot = 0.25;

void Turn(const Quat &orientationQ, const Vec3 &bodyV, Vec3 &worldV) {
  const Vec3 q = {{orientationQ.X, orientationQ.Y, orientationQ.Z}};
  const Vec3 qv = Cross(q, bodyV);
  const Vec3 twice = qv * 2.0;
  const Vec3 qqv = Cross(q, twice);
  worldV = bodyV + twice * orientationQ.W + qqv;
}

void Unturn(const Quat &orientationQ, const Vec3 &worldV, Vec3 &bodyV) {
  const Quat back = {
      .X = -orientationQ.X, .Y = -orientationQ.Y, .Z = -orientationQ.Z, .W = orientationQ.W};
  Turn(back, worldV, bodyV);
}

void Place(const Rigid &body, const Vec3 &atBodyM, Vec3 &worldM) {
  Vec3 turned;
  Turn(body.OrientationQ, atBodyM, turned);
  worldM = body.PositionM + turned;
}

void Carry(const Rigid &body, const Vec3 &atBodyM, Vec3 &worldMs) {
  const Vec3 spun = Cross(body.SpinBodyRadS, atBodyM);
  Vec3 turned;
  Turn(body.OrientationQ, spun, turned);
  worldMs = body.VelocityMs + turned;
}

void Push(Wrench &wrench, const Rigid &body, const Vec3 &atBodyM, const Vec3 &forceN) {
  Vec3 arm;
  Turn(body.OrientationQ, atBodyM, arm);
  const Vec3 torque = Cross(arm, forceN);
  wrench.ForceN = wrench.ForceN + forceN;
  wrench.TorqueNm = wrench.TorqueNm + torque;
}

void Fall(Wrench &wrench, const Rigid &body, const Vec3 &gravityMs2) {
  for (int axis = 0; axis < 3; ++axis) { wrench.ForceN[axis] += body.MassKg * gravityMs2[axis]; }
}

void Step(Rigid &body, const Wrench &wrench, double dtS) {
  if (!(body.MassKg > 0.0) || !(dtS > 0.0)) { return; }

  for (int axis = 0; axis < 3; ++axis) {
    body.VelocityMs[axis] += wrench.ForceN[axis] / body.MassKg * dtS;
    body.PositionM[axis] += body.VelocityMs[axis] * dtS;
  }

  Vec3 torqueBody;
  Unturn(body.OrientationQ, wrench.TorqueNm, torqueBody);
  Vec3 momentum;
  for (int axis = 0; axis < 3; ++axis) {
    momentum[axis] = body.InertiaKgM2[axis] * body.SpinBodyRadS[axis];
  }
  const Vec3 gyroscopic = Cross(body.SpinBodyRadS, momentum);
  for (int axis = 0; axis < 3; ++axis) {
    if (!(body.InertiaKgM2[axis] > 0.0)) { continue; }
    body.SpinBodyRadS[axis] += (torqueBody[axis] - gyroscopic[axis]) / body.InertiaKgM2[axis] * dtS;
  }

  const Quat &q = body.OrientationQ;
  const Vec3 &spin = body.SpinBodyRadS;
  const Quat rate = {.X = 0.5 * (q.W * spin[0] + q.Y * spin[2] - q.Z * spin[1]),
                     .Y = 0.5 * (q.W * spin[1] + q.Z * spin[0] - q.X * spin[2]),
                     .Z = 0.5 * (q.W * spin[2] + q.X * spin[1] - q.Y * spin[0]),
                     .W = -0.5 * (q.X * spin[0] + q.Y * spin[1] + q.Z * spin[2])};

  const Quat turned = {.X = q.X + rate.X * dtS,
                       .Y = q.Y + rate.Y * dtS,
                       .Z = q.Z + rate.Z * dtS,
                       .W = q.W + rate.W * dtS};
  const double square =
      turned.W * turned.W + turned.X * turned.X + turned.Y * turned.Y + turned.Z * turned.Z;
  const double length = std::sqrt(square);
  if (!(length > 0.0)) { return; }
  body.OrientationQ = {.X = turned.X / length,
                       .Y = turned.Y / length,
                       .Z = turned.Z / length,
                       .W = turned.W / length};
}

double EnergyJ(const Rigid &body, const Vec3 &gravityMs2) {
  double energy = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    energy += 0.5 * body.MassKg * body.VelocityMs[axis] * body.VelocityMs[axis];
    energy += 0.5 * body.InertiaKgM2[axis] * body.SpinBodyRadS[axis] * body.SpinBodyRadS[axis];
    energy -= body.MassKg * gravityMs2[axis] * body.PositionM[axis];
  }
  return energy;
}

void Lie(Rigid &body, const Vec3 &aheadM, const Vec3 &upM) {
  Vec3 ahead = aheadM;
  Vec3 up = upM;
  (void)Normalise(up);
  const double along = ahead[0] * up[0] + ahead[1] * up[1] + ahead[2] * up[2];
  for (int axis = 0; axis < 3; ++axis) { ahead[axis] -= along * up[axis]; }
  (void)Normalise(ahead);

  const Vec3 back = {{-ahead[0], -ahead[1], -ahead[2]}};
  const Vec3 right = Cross(up, back);

  const std::array<std::array<double, 3>, 3> m = {
      {{right[0], up[0], back[0]}, {right[1], up[1], back[1]}, {right[2], up[2], back[2]}}};
  const double trace = m[0][0] + m[1][1] + m[2][2];
  Quat q;
  if (trace > 0.0) {
    const double root = std::sqrt(trace + 1.0) * 2.0;
    q.W = kQuarterOfRoot * root;
    q.X = (m[2][1] - m[1][2]) / root;
    q.Y = (m[0][2] - m[2][0]) / root;
    q.Z = (m[1][0] - m[0][1]) / root;
  } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
    const double root = std::sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
    q.W = (m[2][1] - m[1][2]) / root;
    q.X = kQuarterOfRoot * root;
    q.Y = (m[0][1] + m[1][0]) / root;
    q.Z = (m[0][2] + m[2][0]) / root;
  } else if (m[1][1] > m[2][2]) {
    const double root = std::sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
    q.W = (m[0][2] - m[2][0]) / root;
    q.X = (m[0][1] + m[1][0]) / root;
    q.Y = kQuarterOfRoot * root;
    q.Z = (m[1][2] + m[2][1]) / root;
  } else {
    const double root = std::sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
    q.W = (m[1][0] - m[0][1]) / root;
    q.X = (m[0][2] + m[2][0]) / root;
    q.Y = (m[1][2] + m[2][1]) / root;
    q.Z = kQuarterOfRoot * root;
  }
  body.OrientationQ = q;
}

} // namespace outshine::Physics
