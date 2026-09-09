#include <world/EntityRegistry.h>
#include <string>
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  EntityRegistry registry;
  CHECK(!registry.roleOf(kNoEntity), "unopened registry has no implicit body");
  CHECK(registry.open(3), "registry opens");
  const Entity offer = registry.addEntity(Role::Tool);
  const Entity user = registry.addEntity(Role::Mind);
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
  const Entity replacement = registry.addEntity(Role::Assignment);
  CHECK(replacement.Index == user.Index && replacement != user, "fixture reuses the old slot");
  CHECK(!registry.roleOf(user) && registry.roleOf(replacement) == Role::Assignment,
        "slot reuse cannot assign the new role to a stale handle");
  const Seating next{.By = replacement, .At = offer};
  CHECK(registry.seatOf(next) == Seat::Free && registry.claimSeat(next),
        "replacement does not inherit occupancy and can reclaim the released capacity");
  EntityRegistry foreign;
  CHECK(foreign.open(3), "foreign registry opens");
  const Entity outsider = foreign.addEntity(Role::Body);
  CHECK(!registry.roleOf(outsider), "foreign entity has no local role");
  CHECK(registry.seatOf({.By = outsider, .At = offer}) == Seat::Free,
        "foreign claimant cannot observe a local reservation");
  CHECK(!registry.open(0), "fixture records a validation error");
  const std::string error(registry.error());
  CHECK(!registry.roleOf(kNoEntity) && registry.seatOf({}) == Seat::Free &&
            registry.error() == error,
        "invalid queries preserve the previous error");
  registry.remove(offer);
  CHECK(registry.seatOf(next) == Seat::Free, "removed offering entity invalidates its seats");
  CHECK(registry.open(3) && !registry.roleOf(replacement), "reopening invalidates former roles");
  return Report();
}
