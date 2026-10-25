#include "RigidBodyDynamics.h"
#include "RigidBodyState.h"
#include "Vec3.h"
#include "Check.h"
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double kTolerance = 1e-12;
  constexpr double kStepS = 1.0 / 128.0;
  const double halfTurnComponent = std::sqrt(0.5);
  World::RigidBodyState body;
  body.MassKg = 2;
  body.InertiaKgM2 = {{2, 3, 5}};
  body.OrientationQ.Z = halfTurnComponent;
  body.OrientationQ.W = halfTurnComponent;
  Vec3 rotated;
  Physics::RotateVector(body.OrientationQ, {{1, 0, 0}}, rotated);
  CHECK(std::abs(rotated[0]) < kTolerance && std::abs(rotated[1] - 1) < kTolerance &&
            rotated[2] == 0,
        "a right-handed quarter turn sends local X to world Y");
  Vec3 local;
  Physics::InverseRotateVector(body.OrientationQ, rotated, local);
  CHECK(std::abs(local[0] - 1) < kTolerance && std::abs(local[1]) < kTolerance && local[2] == 0,
        "inverse rotation restores the local direction");
  Physics::Wrench force;
  Physics::AccumulateForce(force, body, {.AtBodyM = {{1, 0, 0}}, .Newtons = {{1, 0, 0}}});
  CHECK(std::abs(force.TorqueNm[2] + 1) < kTolerance,
        "world force at the rotated one-metre lever produces negative world Z torque");
  body.SpinBodyRadS = {{1, 2, 3}};
  Physics::Wrench torque;
  torque.TorqueNm = {{1, 2, 3}};
  const Vec3 acceleration = {{-5, 8.0 / 3.0, 1.0 / 5.0}};
  const Vec3 initialSpin = body.SpinBodyRadS;
  Physics::IntegrateBody(body, torque, kStepS);
  for (int axis = 0; axis < 3; ++axis) {
    CHECK(std::abs(body.SpinBodyRadS[axis] - initialSpin[axis] - acceleration[axis] * kStepS) <
              kTolerance,
          "body torque (2,-1,3) minus omega-cross-Iomega (12,-9,2) uses principal inertia");
  }
  const auto &q = body.OrientationQ;
  CHECK(std::abs(q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W - 1) < kTolerance,
        "orientation remains a unit quaternion after the gyroscopic step");
  CHECK(body.PositionM == Vec3{} && body.VelocityMs == Vec3{},
        "pure torque does not create translational motion");
  return Report();
}
