#ifndef OUTSHINE_SIMULATION_PHYSICS_RIGIDBODYDYNAMICS_H
#define OUTSHINE_SIMULATION_PHYSICS_RIGIDBODYDYNAMICS_H

#include "EntityBodyState.h"
#include "RigidBodyState.h"
#include "Quat.h"
#include "Vec3.h"
#include <span>

namespace outshine::Physics {

struct Force {
  Vec3 AtBodyM = {{0.0, 0.0, 0.0}};
  Vec3 Newtons = {{0.0, 0.0, 0.0}};
};

struct Facing {
  Vec3 AheadM = {{0.0, 0.0, -1.0}};
  Vec3 UpM = {{0.0, 1.0, 0.0}};
};

struct Wrench {
  alignas(16) Vec3 ForceN;
  alignas(16) Vec3 TorqueNm;
};

void RotateVector(const Quat &orientationQ, const Vec3 &bodyV, Vec3 &worldV);
void InverseRotateVector(const Quat &orientationQ, const Vec3 &worldV, Vec3 &bodyV);

void TransformBodyPoint(const World::RigidBodyState &body, const Vec3 &atBodyM, Vec3 &worldM);

void SetBodyOrientation(World::RigidBodyState &body, const Facing &faces);
void VelocityAtBodyPoint(const World::RigidBodyState &body, const Vec3 &atBodyM, Vec3 &worldMs);

void AccumulateForce(Wrench &wrench, const World::RigidBodyState &body, const Force &applied);
void AccumulateGravity(Wrench &wrench, const World::RigidBodyState &body, const Vec3 &gravityMs2);

void IntegrateBody(World::RigidBodyState &body, const Wrench &wrench, double dtS);

[[nodiscard]] double MechanicalEnergyJ(const World::RigidBodyState &body, const Vec3 &gravityMs2);

void IntegrateBodies(std::span<World::EntityBodyState> bodies,
                     double stepSeconds,
                     const Vec3 &gravityMs2);

}

#endif
