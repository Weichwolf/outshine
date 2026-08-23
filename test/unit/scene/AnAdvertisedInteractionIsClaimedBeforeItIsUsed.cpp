#include <cstdio>

#include "Check.h"

#include "Store.h"

using outshine::Entity;
using outshine::Role;
using outshine::Seat;
using outshine::Store;
using outshine::Tag;
namespace tags = outshine::tags;


int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Store scene;
  CHECK(scene.Open(16), "a store opens");

  const Entity pump = scene.Add(Role::Body);
  const Entity first = scene.Add(Role::Mind);
  const Entity second = scene.Add(Role::Mind);

  CHECK(!scene.Claim(first, pump), "what advertises nothing cannot be claimed");
  CHECK(scene.Offer(pump, tags::OffersRefuel, 1),
        "**AN OBJECT ADVERTISES AS DATA**: an activity tag and a seat count, never a script -- "
        "the agent's own runtime is what executes (the Sims shipped the other way and every "
        "object became a program; Smart Objects is the correction)");

  Entity found[4];
  CHECK(scene.Offering(tags::Offers, found, 4) == 1 && found[0] == pump,
        "and it is found by the PARENT activity tag, so a search for anything offered uses the "
        "same prefix algebra as capability");

  CHECK(scene.Claim(first, pump), "the first mind claims the one seat");
  CHECK(!scene.Claim(second, pump),
        "**FIND AND USE ARE SEPARATED BY TRAVEL TIME**, which is why the seat is a reservation: "
        "the second mind is refused at claim, not surprised at arrival");
  CHECK(!scene.Use(second, pump), "use stands only on a claim");
  CHECK(scene.Use(first, pump) && scene.SeatOf(first, pump) == Seat::Occupied,
        "the claimant uses, and the seat is occupied");
  CHECK(scene.Release(first, pump) && scene.SeatOf(first, pump) == Seat::Free,
        "released is free again");
  CHECK(scene.Claim(second, pump), "and the next claimant takes it");

  scene.Remove(second);
  CHECK(scene.Claim(first, pump),
        "**A DEAD CLAIMANT HOLDS NO SEAT**: the generation on its handle died with it, so the "
        "seat frees itself at the next claim rather than leaking forever");

  scene.Remove(pump);
  const Entity tenant = scene.Add(Role::Body);
  CHECK(tenant.Index == pump.Index,
        "the pool hands the dead pump's slot to the next tenant");
  Entity offering[4];
  CHECK(scene.Offering(tags::Offers, offering, 4) == 0,
        "**A FRESH ENTITY ADVERTISES NOTHING, WHATEVER ITS SLOT HELD BEFORE**: the offer died "
        "with its owner, so the tenant is no accidental fuel pump (board:1588's repro)");
  CHECK(scene.Offer(tenant, tags::OffersRefuel, 2),
        "and it may advertise on its own terms, because the seats were reset with the slot");

  {
    // the runtime verbs refuse WITHOUT an allocation: a full pump refuses every passing
    // mind every tick, and the refusal aliases its static text -- the same words at the
    // same address on every ask
    const Entity third = scene.Add(Role::Mind);
    const Entity fourth = scene.Add(Role::Mind);
    const Entity fifth = scene.Add(Role::Mind);
    CHECK(scene.Claim(third, tenant) && scene.Claim(fourth, tenant),
          "two minds take the tenant's two seats");
    CHECK(!scene.Claim(fifth, tenant), "and the third ask meets a full offer");
    const char *said = scene.Error().data();
    CHECK(!scene.Claim(fifth, tenant) && scene.Error().data() == said,
          "**A RUNTIME REFUSAL COSTS NO ALLOCATION**: the second identical refusal aliases "
          "the SAME static text -- a built string would live somewhere new (board:1722)");

    // and a dead claimant's retained handle moves nothing: Use holds Claim's bar
    const Entity keptHandle = fourth;
    scene.Remove(fourth);
    CHECK(!scene.Use(keptHandle, tenant),
          "**A DEAD CLAIMANT CANNOT USE**: the retained handle's generation is stale and "
          "use demands a standing claimant, the bar claim always held (board:1722)");
    CHECK(!scene.Release(keptHandle, tenant), "nor release");
    CHECK(scene.Claim(fifth, tenant),
          "and the dead claim's seat frees at the next claim, as before");
  }

  Covers("II.3 a shared interaction is advertised as data and reserved before it is used: "
         "Free -> Claimed -> Occupied -> Free, one seat per claimant, and a dead claimant "
         "frees its seat by generation");
  return Report();
}
