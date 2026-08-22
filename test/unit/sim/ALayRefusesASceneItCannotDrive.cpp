#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include <outshine/Assembled.h>
#include <outshine/Column.h>
#include <outshine/Register.h>
#include <outshine/Scenario.h>
#include <outshine/Store.h>

#include "Journey.h"
#include "Sink.h"
#include "Transport.h"

using outshine::Assembled;
using outshine::Column;
using outshine::Drive;
using outshine::Entity;
using outshine::Relation;
using outshine::Role;
using outshine::Store;
using outshine::Vehicle;
using outshine::WorldSettings;
using outshine::Sim::Journey;
using outshine::Sim::Provision;
using outshine::Sim::Sink;

namespace {

class Quiet : public Sink {
public:
  void Number(const char *, double, const char *) override {}
  void Claim(bool, const char *) override {}
  void Near(double, double, double, const char *, const char *) override {}
  void Say(const std::string &) override {}
};

class NoWire : public outshine::Data::Transport {
public:
  size_t Asked = 0;
  [[nodiscard]] outshine::Data::Ticket Begin(const std::string &) override {
    ++Asked;
    return {};
  }
  [[nodiscard]] outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Unreachable();
  }
  void Cancel(outshine::Data::Ticket) override {}
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Store scene;
  Column<Vehicle> vehicles;
  Column<Drive> drives;
  CHECK(scene.Open(8) && vehicles.Open(scene) && drives.Open(scene), "the graph opens");

  Assembled cast;
  cast.PlayerBody = scene.Add(Role::Body);
  cast.PlayerMind = scene.Add(Role::Mind);
  cast.Assignment = scene.Add(Role::Assignment);
  const Entity elsewhere = scene.Add(Role::Assignment);
  const Entity nav = scene.Add(Role::Tool);
  Vehicle car;
  car.MassKg = 1000.0;
  CHECK(vehicles.Put(cast.PlayerBody, car), "a body carries its declaration in the column");
  Drive to;
  to.Declared = true;
  CHECK(drives.Put(cast.Assignment, to), "and the assignment carries the coordinates");
  CHECK(scene.Link(cast.PlayerMind, Relation::Uses, nav),
        "the mind uses its tool -- Assigned requires Uses");
  CHECK(scene.Link(cast.PlayerMind, Relation::Assigned, elsewhere),
        "the mind is Assigned a DIFFERENT assignment -- the mislinked scene");

  Quiet quiet;
  NoWire wire;
  Journey journey;
  const WorldSettings world;
  CHECK(!journey.Lay(scene, cast, vehicles, drives, world, wire,
                     Provision{"/tmp/outshine-drive-cache", "src/assets"}, quiet),
        "**A MIND ASSIGNED ELSEWHERE REFUSES THE LAY INSTEAD OF LOGGING AND DRIVING ANYWAY** -- "
        "the claim and the refusal are one predicate, not a printed claim and a weaker guard");
  CHECK(wire.Asked == 0, "and the refusal happens before the journey asks the world for anything");

  Store held;
  Column<Vehicle> heldCars;
  Column<Drive> heldDrives;
  Assembled right;
  CHECK(held.Open(8) && heldCars.Open(held) && heldDrives.Open(held), "a second graph opens");
  right.PlayerBody = held.Add(Role::Body);
  right.PlayerMind = held.Add(Role::Mind);
  right.Assignment = held.Add(Role::Assignment);
  const Entity heldNav = held.Add(Role::Tool);
  CHECK(heldCars.Put(right.PlayerBody, car) && heldDrives.Put(right.Assignment, to) &&
            held.Link(right.PlayerMind, Relation::Uses, heldNav) &&
            held.Link(right.PlayerMind, Relation::Assigned, right.Assignment),
        "and assembles correctly");
  Journey second;
  CHECK(!second.Lay(held, right, heldCars, heldDrives, world, wire, Provision{"", ""}, quiet),
        "an unprovisioned journey refuses at the same head");
  CHECK(wire.Asked == 0, "still without asking the world");

  Covers("II.14 Journey::Lay refuses at its head: an unprovisioned caller, a car without a "
         "declaration, or a mind whose Assigned pair does not target the cast's assignment "
         "all end the lay before any provider is asked -- refusal at assembly, not a log line");
  return Report();
}
