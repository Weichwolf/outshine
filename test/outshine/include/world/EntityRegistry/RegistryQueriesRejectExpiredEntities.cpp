#include <world/EntityRegistry.h>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  EntityRegistry registry;
  CHECK(!registry.roleOf(kNoEntity), "unopened registry has no implicit body");
  CHECK(registry.open(3), "registry opens");
  const auto offered = registry.addEntity(Role::Tool);
  const auto added = registry.addEntity(Role::Mind);
  CHECK(offered && added, "fixture entities are allocated");
  if (!offered || !added) { return Report(); }
  const Entity offer = *offered;
  const Entity user = *added;
  CHECK(registry.roleOf(offer) == Role::Tool && registry.roleOf(user) == Role::Mind,
        "live roles match their declarations");
  CHECK(registry.offerSeats(offer, tags::Offers, 1), "one reservation is available");
  const Seating reservation{.By = user, .At = offer};
  CHECK(registry.claimSeat(reservation) && registry.seatOf(reservation) == Seat::Claimed,
        "claim is visible before occupation");
  CHECK(registry.takeSeat(reservation) && registry.seatOf(reservation) == Seat::Occupied,
        "occupied reservation is visible while both entities live");
  registry.remove(user);
  CHECK(!registry.roleOf(user), "removed entity has no role");
  CHECK(registry.seatOf(reservation) == Seat::Free,
        "removed claimant cannot retain an observable occupied seat");
  const auto replaced = registry.addEntity(Role::Assignment);
  CHECK(replaced, "fixture replacement is allocated");
  if (!replaced) { return Report(); }
  const Entity replacement = *replaced;
  CHECK(replacement.Index == user.Index && replacement != user, "fixture reuses the old slot");
  CHECK(!registry.roleOf(user) && registry.roleOf(replacement) == Role::Assignment,
        "slot reuse cannot assign the new role to a stale handle");
  const Seating next{.By = replacement, .At = offer};
  CHECK(registry.seatOf(next) == Seat::Free && registry.claimSeat(next),
        "replacement does not inherit occupancy and can reclaim the released capacity");
  EntityRegistry foreign;
  CHECK(foreign.open(3), "foreign registry opens");
  const auto addedOutsider = foreign.addEntity(Role::Body);
  CHECK(addedOutsider, "foreign fixture entity is allocated");
  if (!addedOutsider) { return Report(); }
  const Entity outsider = *addedOutsider;
  CHECK(!registry.roleOf(outsider), "foreign entity has no local role");
  CHECK(registry.seatOf({.By = outsider, .At = offer}) == Seat::Free,
        "foreign claimant cannot observe a local reservation");
  const auto rejected = registry.open(0);
  CHECK(!rejected && !rejected.error().Message.empty(), "invalid capacity carries its own error");
  CHECK(!registry.roleOf(kNoEntity) && registry.seatOf({}) == Seat::Free,
        "queries do not overwrite a prior operation result");
  registry.remove(offer);
  CHECK(registry.seatOf(next) == Seat::Free, "removed offering entity invalidates its seats");
  CHECK(registry.open(3) && !registry.roleOf(replacement), "reopening invalidates former roles");
  return Report();
}
