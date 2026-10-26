#include "RigidBodyState.h"
#include "Vec3.h"
#include "RigidBodyDynamics.h"
#include "EntityBodyState.h"
#include "Entity.h"
#include "EntityState.h"
#include <vector>
#include <utility>
#include "Check.h"
#include <array>
#include <cstddef>
#include <print>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr Vec3 gravity{{0, -10, 0}};
  constexpr double stepS = 0.5;
  constexpr std::array massesKg{1.0, 10.0, 0.0};
  World::EntityState world;
  CHECK(world.Open(massesKg.size()), "the physics world opens");
  std::vector<World::EntityBodyState> initial;
  std::array<Entity, 3> owners;
  for (size_t at = 0; at < massesKg.size(); ++at) {
    const auto entity = world.Registry().addEntity();
    CHECK(entity, "native physics identity exists");
    if (!entity) { return Report(); }
    owners[at] = *entity;
    World::EntityBodyState body;
    body.Owner = *entity;
    body.Motion.MassKg = massesKg[at];
    initial.push_back(body);
  }
  CHECK(world.SetBodies(std::move(initial)), "the native body writer is initialized");
  const auto bodies = world.Bodies();
  Physics::IntegrateBodies(world, stepS, gravity);
  for (size_t at = 0; at < bodies.size(); ++at) {
    const auto &body = bodies[at];
    CHECK(body.Owner == owners[at], "native entity identity survives batch integration");
    const bool movable = massesKg[at] > 0;
    CHECK(body.Motion.VelocityMs[1] == (movable ? gravity[1] * stepS : 0) &&
              body.Motion.PositionM[1] == (movable ? gravity[1] * stepS * stepS : 0),
          "v=g*dt and x=g*dt^2 are independent of positive mass; static bodies stay fixed");
    CHECK(body.Motion.PositionM[0] == 0 && body.Motion.PositionM[2] == 0 &&
              body.Motion.OrientationQ.W == 1,
          "gravity does not change transverse position or orientation");
  }
  Physics::IntegrateBodies(world, 0, gravity);
  CHECK(bodies[0].Motion.PositionM[1] == gravity[1] * stepS * stepS &&
            bodies[0].Motion.VelocityMs[1] == gravity[1] * stepS,
        "a zero-time step preserves the current physical state");
  Physics::IntegrateBodies(world, stepS, gravity);
  CHECK(bodies[0].Motion.PositionM[1] == 3 * gravity[1] * stepS * stepS &&
            bodies[1].Motion.PositionM[1] == bodies[0].Motion.PositionM[1] &&
            bodies[2].Motion.PositionM[1] == 0,
        "two semi-implicit steps give x=3*g*dt^2 while a static body remains fixed");
  World::EntityState empty;
  Physics::IntegrateBodies(empty, stepS, gravity);
  std::println("native_body_bytes={} entity_body_bytes={}",
               sizeof(World::RigidBodyState),
               sizeof(World::EntityBodyState));
  return Report();
}
