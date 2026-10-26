#include "RigidBodyDynamics.h"
#include "RigidBodyState.h"
#include "Vec3.h"
#include "Check.h"
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double massKg = 2;
  constexpr double inertiaKgM2 = 4;
  constexpr double forceN = 8;
  constexpr double stepS = 0.25;
  constexpr double tolerance = 1e-12;
  World::RigidBodyState body;
  body.MassKg = massKg;
  body.InertiaKgM2 = {{inertiaKgM2, inertiaKgM2, inertiaKgM2}};
  Physics::Wrench wrench;
  Physics::AccumulateForce(wrench, body, {.AtBodyM = {{1, 0, 0}}, .Newtons = {{0, forceN, 0}}});
  CHECK(wrench.ForceN[1] == forceN && wrench.TorqueNm[2] == forceN && wrench.TorqueNm[0] == 0 &&
            wrench.TorqueNm[1] == 0,
        "a one-metre lever arm produces r cross F in newton-metres");
  Physics::IntegrateBody(body, wrench, stepS);
  const double speedMs = forceN / massKg * stepS;
  const double spinRadS = forceN / inertiaKgM2 * stepS;
  CHECK(body.VelocityMs[1] == speedMs && body.PositionM[1] == speedMs * stepS &&
            body.SpinBodyRadS[2] == spinRadS,
        "linear and angular acceleration use their own mass and inertia");
  const double halfAngle = std::atan(spinRadS * stepS * 0.5);
  CHECK(std::abs(body.OrientationQ.Z - std::sin(halfAngle)) < tolerance &&
            std::abs(body.OrientationQ.W - std::cos(halfAngle)) < tolerance,
        "normalized Euler rotation is the quaternion for twice atan(omega*dt/2)");
  const double kineticJ =
      0.5 * massKg * speedMs * speedMs + 0.5 * inertiaKgM2 * spinRadS * spinRadS;
  CHECK(Physics::MechanicalEnergyJ(body, {}) == kineticJ,
        "kinetic energy includes translation and body-frame rotation");
  const auto before = body;
  Physics::IntegrateBody(body, wrench, 0);
  CHECK(body.PositionM[1] == before.PositionM[1] && body.OrientationQ.Z == before.OrientationQ.Z &&
            body.SpinBodyRadS[2] == before.SpinBodyRadS[2],
        "zero time does not advance force-driven state");
  return Report();
}
