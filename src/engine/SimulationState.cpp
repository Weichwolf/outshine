#include "SimulationState.h"
#include <cassert>
#include <scene/Scene.h>
#include <scenario/Scenario.h>
#include "Rigid.h"
#include "math/Vec3.h"

namespace outshine {

void SimulationState::PrepareBodies() {
  DynamicBodies.reserve(Stood.Bodies.size());
  for (const Entity entity : Stood.Bodies) {
    const Scenario::Body *definition = Bodies.Get(entity);
    assert(definition != nullptr);
    if (!definition->Placed) { continue; }
    Physics::Rigid motion;
    motion.MassKg = definition->MassKg;
    motion.PositionM = definition->Stands.AtM;
    motion.InertiaKgM2 = definition->InertiaKgM2;
    motion.OrientationQ = definition->Stands.Facing;
    DynamicBodies.push_back({.Owner = entity, .Motion = motion});
  }
}

void SimulationState::Integrate(double stepSeconds, const Vec3 &gravityMs2) {
  for (auto &body : DynamicBodies) {
    Physics::Wrench force;
    Physics::Fall(force, body.Motion, gravityMs2);
    Physics::Step(body.Motion, force, stepSeconds);
  }
}

}
