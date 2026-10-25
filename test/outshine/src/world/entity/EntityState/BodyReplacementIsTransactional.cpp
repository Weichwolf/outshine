#include "EntityState.h"
#include "EntityBodyState.h"
#include "Entity.h"
#include "Check.h"
#include <cstddef>
#include <vector>

namespace {
using namespace outshine;

World::EntityBodyState Body(Entity owner, double eastM) {
  World::EntityBodyState body;
  body.Owner = owner;
  body.Motion.PositionM[0] = eastM;
  return body;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  World::EntityState state;
  CHECK(state.Open(2), "native World storage opens without a scenario or simulation");
  const auto left = state.Registry().addEntity(Role::Body);
  const auto right = state.Registry().addEntity(Role::Body);
  CHECK(left && right, "native identities are allocated");
  if (!left || !right) { return Report(); }
  CHECK(state.SetBodies({Body(*left, -1)}), "a subset of native entities has physical state");
  CHECK(state.Body(*left) && state.Body(*right) == nullptr && state.Body(kNoEntity) == nullptr,
        "live identities without body state are not fabricated");
  CHECK(state.SetBodies({Body(*right, 1), Body(*left, -1)}),
        "physical rows may arrive independently of registry order");
  CHECK(state.Body(*left)->PositionM[0] == -1 && state.Body(*right)->PositionM[0] == 1,
        "constant-time lookup resolves the correct entity rather than a row ordinal");
  CHECK(state.EditBody(*right, [](World::RigidBodyState &body) { body.PositionM[0] = 2; }),
        "the native writer edits an owned body");
  CHECK(state.Body(*right)->PositionM[0] == 2,
        "simulation writes directly into the authoritative native body row");
  const auto *previous = state.Bodies().data();
  const auto duplicate = state.SetBodies({Body(*left, 0), Body(*left, 1)});
  CHECK(!duplicate && duplicate.error() == World::EntityStateError::DuplicateBody &&
            state.Bodies().data() == previous && state.Body(*right)->PositionM[0] == 2,
        "duplicate ownership preserves both prior physical rows and their index");
  const auto oversized = state.SetBodies({Body(*left, 0), Body(*right, 0), Body(*left, 0)});
  CHECK(!oversized && oversized.error() == World::EntityStateError::CapacityExceeded &&
            state.Bodies().data() == previous,
        "capacity rejection does not replace the current world state");
  World::EntityState other;
  CHECK(other.Open(1), "a second world has independent identity ownership");
  const auto foreign = other.Registry().addEntity(Role::Body);
  CHECK(foreign.has_value(), "foreign entity exists");
  if (!foreign) { return Report(); }
  const auto invalid = state.SetBodies({Body(*foreign, 0)});
  CHECK(!invalid && invalid.error() == World::EntityStateError::InvalidEntity &&
            state.Bodies().data() == previous && state.Body(*foreign) == nullptr,
        "foreign identities cannot steal native body rows");
  CHECK(!state.Open(1) && state.Bodies().data() == previous,
        "a bound world cannot reopen its registry beneath component columns");
  state.Registry().remove(*right);
  CHECK(state.Body(*right) == nullptr, "dead identities no longer expose physical state");
  const auto replacement = state.Registry().addEntity(Role::Body);
  CHECK(replacement && replacement->Index == right->Index && *replacement != *right,
        "reused registry slots get a new entity generation");
  if (!replacement) { return Report(); }
  CHECK(state.Body(*replacement) == nullptr, "generation reuse never inherits an old body");
  CHECK(state.SetBodies({Body(*left, -1), Body(*replacement, 2)}) && state.Body(*replacement) &&
            state.Body(*replacement)->PositionM[0] == 2 && state.Body(*right) == nullptr,
        "a replacement publishes its own body while old handles remain invalid");
  CHECK(state.SetBodies({}) && state.Bodies().empty() && state.Body(*left) == nullptr,
        "empty publication releases physical state without destroying logical entities");
  World::EntityState empty;
  CHECK(empty.Open(0) && empty.SetBodies({}) && empty.Body(kNoEntity) == nullptr,
        "an empty native world is valid without allocating logical entities");
  return Report();
}
