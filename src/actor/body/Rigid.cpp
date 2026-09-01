#include <array>
#include <span>

#include "Rigid.h"

#include <cmath>

#include "Vec3.h"

namespace outshine::Physics {

using outshine::Cross;

void Turn(std::span<const double, 4> orientationQ,
          std::span<const double, 3> bodyV,
          std::span<double, 3> worldV) {
  const double w = orientationQ[0];
  const std::array<double, 3> q = {orientationQ[1], orientationQ[2], orientationQ[3]};
  std::array<double, 3> qv{};
  Cross(q, bodyV, qv);
  std::array<double, 3> qqv{};
  const std::array<double, 3> twice = {2.0 * qv[0], 2.0 * qv[1], 2.0 * qv[2]};
  Cross(q, twice, qqv);
  for (int axis = 0; axis < 3; ++axis) { worldV[axis] = bodyV[axis] + w * twice[axis] + qqv[axis]; }
}

void Unturn(std::span<const double, 4> orientationQ,
            std::span<const double, 3> worldV,
            std::span<double, 3> bodyV) {
  const std::array<double, 4> back = {
      orientationQ[0], -orientationQ[1], -orientationQ[2], -orientationQ[3]};
  Turn(back, worldV, bodyV);
}

void Place(const Rigid &body, std::span<const double, 3> atBodyM, std::span<double, 3> worldM) {
  std::array<double, 3> turned{};
  Turn(body.OrientationQ, atBodyM, turned);
  for (int axis = 0; axis < 3; ++axis) { worldM[axis] = body.PositionM[axis] + turned[axis]; }
}

void Carry(const Rigid &body, std::span<const double, 3> atBodyM, std::span<double, 3> worldMs) {
  std::array<double, 3> spun{};
  Cross(body.SpinBodyRadS, atBodyM, spun);
  std::array<double, 3> turned{};
  Turn(body.OrientationQ, spun, turned);
  for (int axis = 0; axis < 3; ++axis) { worldMs[axis] = body.VelocityMs[axis] + turned[axis]; }
}

void Push(Wrench &wrench,
          const Rigid &body,
          std::span<const double, 3> atBodyM,
          std::span<const double, 3> forceN) {
  std::array<double, 3> arm{};
  Turn(body.OrientationQ, atBodyM, arm);
  std::array<double, 3> torque{};
  Cross(arm, forceN, torque);
  for (int axis = 0; axis < 3; ++axis) {
    wrench.ForceN[axis] += forceN[axis];
    wrench.TorqueNm[axis] += torque[axis];
  }
}

void Fall(Wrench &wrench, const Rigid &body, std::span<const double, 3> gravityMs2) {
  for (int axis = 0; axis < 3; ++axis) { wrench.ForceN[axis] += body.MassKg * gravityMs2[axis]; }
}

void Step(Rigid &body, const Wrench &wrench, double dtS) {
  if (!(body.MassKg > 0.0) || !(dtS > 0.0)) { return; }

  for (int axis = 0; axis < 3; ++axis) {
    body.VelocityMs[axis] += wrench.ForceN[axis] / body.MassKg * dtS;
    body.PositionM[axis] += body.VelocityMs[axis] * dtS;
  }

  std::array<double, 3> torqueBody{};
  Unturn(body.OrientationQ, wrench.TorqueNm, torqueBody);
  std::array<double, 3> momentum{};
  for (int axis = 0; axis < 3; ++axis) {
    momentum[axis] = body.InertiaKgM2[axis] * body.SpinBodyRadS[axis];
  }
  std::array<double, 3> gyroscopic{};
  Cross(body.SpinBodyRadS, momentum, gyroscopic);
  for (int axis = 0; axis < 3; ++axis) {
    if (!(body.InertiaKgM2[axis] > 0.0)) { continue; }
    body.SpinBodyRadS[axis] += (torqueBody[axis] - gyroscopic[axis]) / body.InertiaKgM2[axis] * dtS;
  }

  const std::array<double, 4> &q = body.OrientationQ;
  const std::array<double, 3> &spin = body.SpinBodyRadS;
  const std::array<double, 4> rate = {-0.5 * (q[1] * spin[0] + q[2] * spin[1] + q[3] * spin[2]),
                                      0.5 * (q[0] * spin[0] + q[2] * spin[2] - q[3] * spin[1]),
                                      0.5 * (q[0] * spin[1] + q[3] * spin[0] - q[1] * spin[2]),
                                      0.5 * (q[0] * spin[2] + q[1] * spin[1] - q[2] * spin[0])};

  std::array<double, 4> turned{};
  double square = 0.0;
  for (int part = 0; part < 4; ++part) {
    turned[part] = body.OrientationQ[part] + rate[part] * dtS;
    square += turned[part] * turned[part];
  }
  const double length = std::sqrt(square);
  if (!(length > 0.0)) { return; }
  for (int part = 0; part < 4; ++part) { body.OrientationQ[part] = turned[part] / length; }
}

double EnergyJ(const Rigid &body, std::span<const double, 3> gravityMs2) {
  double energy = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    energy += 0.5 * body.MassKg * body.VelocityMs[axis] * body.VelocityMs[axis];
    energy += 0.5 * body.InertiaKgM2[axis] * body.SpinBodyRadS[axis] * body.SpinBodyRadS[axis];
    energy -= body.MassKg * gravityMs2[axis] * body.PositionM[axis];
  }
  return energy;
}

namespace {

void Unit(std::span<double, 3> v) {
  const double length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (length > 0.0) {
    for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  }
}

} // namespace

void Lie(Rigid &body, std::span<const double, 3> aheadM, std::span<const double, 3> upM) {
  std::array<double, 3> ahead = {aheadM[0], aheadM[1], aheadM[2]};
  std::array<double, 3> up = {upM[0], upM[1], upM[2]};
  Unit(up);
  const double along = ahead[0] * up[0] + ahead[1] * up[1] + ahead[2] * up[2];
  for (int axis = 0; axis < 3; ++axis) { ahead[axis] -= along * up[axis]; }
  Unit(ahead);

  const std::array<double, 3> back = {-ahead[0], -ahead[1], -ahead[2]};
  const std::array<double, 3> right = {up[1] * back[2] - up[2] * back[1],
                                       up[2] * back[0] - up[0] * back[2],
                                       up[0] * back[1] - up[1] * back[0]};

  const std::array<std::array<double, 3>, 3> m = {
      {{right[0], up[0], back[0]}, {right[1], up[1], back[1]}, {right[2], up[2], back[2]}}};
  const double trace = m[0][0] + m[1][1] + m[2][2];
  std::array<double, 4> q{};
  if (trace > 0.0) {
    const double root = std::sqrt(trace + 1.0) * 2.0;
    q[0] = 0.25 * root;
    q[1] = (m[2][1] - m[1][2]) / root;
    q[2] = (m[0][2] - m[2][0]) / root;
    q[3] = (m[1][0] - m[0][1]) / root;
  } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
    const double root = std::sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
    q[0] = (m[2][1] - m[1][2]) / root;
    q[1] = 0.25 * root;
    q[2] = (m[0][1] + m[1][0]) / root;
    q[3] = (m[0][2] + m[2][0]) / root;
  } else if (m[1][1] > m[2][2]) {
    const double root = std::sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
    q[0] = (m[0][2] - m[2][0]) / root;
    q[1] = (m[0][1] + m[1][0]) / root;
    q[2] = 0.25 * root;
    q[3] = (m[1][2] + m[2][1]) / root;
  } else {
    const double root = std::sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
    q[0] = (m[1][0] - m[0][1]) / root;
    q[1] = (m[0][2] + m[2][0]) / root;
    q[2] = (m[1][2] + m[2][1]) / root;
    q[3] = 0.25 * root;
  }
  for (int part = 0; part < 4; ++part) { body.OrientationQ[part] = q[part]; }
}

} // namespace outshine::Physics
