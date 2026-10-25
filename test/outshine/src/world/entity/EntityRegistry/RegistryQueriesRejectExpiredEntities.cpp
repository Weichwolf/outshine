#include <EntityRegistry.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  EntityRegistry registry;
  CHECK(!registry.roleOf(kNoEntity), "unopened registry has no implicit body");
  CHECK(registry.open(3), "registry opens");
  const auto addedTool = registry.addEntity(Role::Tool);
  const auto added = registry.addEntity(Role::Mind);
  CHECK(addedTool && added, "fixture entities are allocated");
  if (!addedTool || !added) { return Report(); }
  const Entity tool = *addedTool;
  const Entity user = *added;
  CHECK(registry.roleOf(tool) == Role::Tool && registry.roleOf(user) == Role::Mind,
        "live roles match their declarations");
  registry.remove(user);
  CHECK(!registry.roleOf(user), "removed entity has no role");
  const auto replaced = registry.addEntity(Role::Assignment);
  CHECK(replaced, "fixture replacement is allocated");
  if (!replaced) { return Report(); }
  const Entity replacement = *replaced;
  CHECK(replacement.Index == user.Index && replacement != user, "fixture reuses the old slot");
  CHECK(!registry.roleOf(user) && registry.roleOf(replacement) == Role::Assignment,
        "slot reuse cannot assign the new role to a stale handle");
  EntityRegistry foreign;
  CHECK(foreign.open(3), "foreign registry opens");
  const auto addedOutsider = foreign.addEntity(Role::Body);
  CHECK(addedOutsider, "foreign fixture entity is allocated");
  if (!addedOutsider) { return Report(); }
  const Entity outsider = *addedOutsider;
  CHECK(!registry.roleOf(outsider), "foreign entity has no local role");
  const auto rejected = registry.open(0);
  CHECK(!rejected && !rejected.error().Message.empty(), "invalid capacity carries its own error");
  CHECK(!registry.roleOf(kNoEntity), "queries do not overwrite a prior operation result");
  registry.remove(tool);
  CHECK(registry.open(3) && !registry.roleOf(replacement), "reopening invalidates former roles");
  return Report();
}
