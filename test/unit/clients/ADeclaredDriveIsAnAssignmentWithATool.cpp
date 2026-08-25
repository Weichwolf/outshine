#include <cstdio>
#include <string>

#include "Check.h"

#include "Assembly.h"
#include "ScenarioRead.h"

using outshine::Assembled;
using outshine::Assemble;
using outshine::Entity;
using outshine::kNoEntity;
using outshine::ReadScenario;
using outshine::Relation;
using outshine::Role;
using outshine::Scenario;
using outshine::Store;

namespace {

const char *kDeclared = R"(<?xml version="1.0" encoding="utf-8"?>
<scenario name="a drive" version="1">
  <vehicle name="car" massKg="1000" wheelbaseM="2.5" turningCircleM="11.0" trackM="1.5">
    <tyre grip="0.9" radiusM="0.3" corneringNPerRad="50000" relaxationM="0.4"/>
    <drive peakTorqueNm="300" finalDrive="3.0"/>
    <brake peakTorqueNm="5000"/>
  </vehicle>
  <player is="car"/>
  <drive fromLat="48.1371" fromLon="11.5754" toLat="53.5503" toLon="9.9920"/>
</scenario>
)";

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Scenario declared;
  std::string error;
  const bool wasRead = ReadScenario(kDeclared, std::char_traits<char>::length(kDeclared), declared, error);
  if (!wasRead) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(wasRead, "a scenario declaring a vehicle, a player and a drive reads");
  CHECK(declared.Driven.Declared,
        "**A DRIVE IS TWO COORDINATES AND NOTHING MORE**: the zoom the ways are read at is the "
        "finest a declared vector source carries, and the engine reads it from the source. A "
        "scenario that named it was naming a number the system already owned, and could name a "
        "WRONG one -- a coarser zoom is a coarser quantumM and therefore a looser fit, with "
        "nothing saying so (board:1859)");

  Store scene;
  outshine::Column<outshine::Vehicle> vehicles;
  outshine::Column<outshine::Drive> drives;
  CHECK(scene.Open(8) && vehicles.Open(scene) && drives.Open(scene), "the graph opens");
  outshine::Column<outshine::Traits> kinds;
  CHECK(kinds.Open(scene), "and one for the kinds' resolved traits");
  Assembled stood;
  const bool assembled = Assemble(declared, scene, vehicles, drives, kinds, stood, error);
  if (!assembled) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(assembled, "and assembles through the one API");

  CHECK(scene.Alive(stood.Nav) && scene.RoleOf(stood.Nav) == Role::Tool,
        "**NAVIGATION IS A TOOL THE ASSEMBLY HANDS OVER**, never built into the mind -- the "
        "CARLA grammar of the reference design");
  CHECK(scene.TargetOf(stood.PlayerMind, Relation::Uses) == stood.Nav,
        "the mind uses it");
  CHECK(scene.TargetOf(stood.PlayerMind, Relation::Assigned) == stood.Assignment,
        "**AND THE DRIVE IS AN ASSIGNMENT**: an entity the mind is Assigned, which the rule "
        "only allowed BECAUSE the mind holds a way to navigate -- the Requires trait did the "
        "ordering, not this test");
  const outshine::Drive *carried = drives.Get(stood.Assignment);
  CHECK(carried != nullptr && carried->FromLatDeg == 48.1371 && carried->ToLonDeg == 9.9920,
        "the two coordinates ride on the assignment as data a system will read");

  Scenario mindless = declared;
  mindless.Played.Is.clear();
  Store lone;
  outshine::Column<outshine::Vehicle> loneV;
  outshine::Column<outshine::Drive> loneD;
  outshine::Column<outshine::Traits> loneK;
  CHECK(lone.Open(8) && loneV.Open(lone) && loneD.Open(lone) && loneK.Open(lone),
        "a second graph opens");
  Assembled nobody;
  CHECK(!Assemble(mindless, lone, loneV, loneD, loneK, nobody, error),
        "**A DRIVE WITHOUT A MIND IS REFUSED AT ASSEMBLY**: somebody must take the assignment, "
        "and the refusal says to declare a player");

  {
    const char *const spellsZoom =
        "<scenario name=\"zoomed\">"
        "<drive fromLat=\"48.1\" fromLon=\"11.5\" toLat=\"52.5\" toLon=\"13.4\" zoom=\"10\"/>"
        "</scenario>";
    Scenario refused;
    std::string why;
    const bool read = ReadScenario(spellsZoom, std::char_traits<char>::length(spellsZoom),
                                   refused, why);
    std::printf("NOTE a drive that spells zoom says: '%.120s'\n", why.c_str());
    CHECK(!read && why.find("zoom") != std::string::npos,
          "**AND A DRIVE THAT SPELLS A ZOOM IS REFUSED BY NAME**: an attribute the grammar does "
          "not declare used to be read as an absent one and silently defaulted, so a scenario "
          "could name a number the engine no longer reads and never learn that it was ignored "
          "-- an orphaned override refuses loudly (board:1859)");
  }

  Covers("II.9 a declared drive assembles as the actor chain end to end: mind Uses a nav tool, "
         "mind Assigned an assignment carrying the two coordinates, ordered by the rules and "
         "refused without a mind");
  {
    Scenario going = declared;
    going.Driven.FromLatDeg = going.Driven.ToLatDeg = 48.0;
    going.Driven.FromLonDeg = going.Driven.ToLonDeg = 11.0;
    Store s;
    outshine::Column<outshine::Vehicle> v;
    outshine::Column<outshine::Drive> d;
    outshine::Column<outshine::Traits> k;
    Assembled a;
    std::string coincideWhy;
    CHECK(s.Open(8) && v.Open(s) && d.Open(s) && k.Open(s) &&
              !Assemble(going, s, v, d, k, a, coincideWhy) &&
              coincideWhy.find("coincide") != std::string::npos,
          "a drive whose ends coincide refuses at assembly naming the point -- a zoom "
          "without a base route is a layer over nothing (board:1689 delivering 1684's "
          "claimed proof)");
  }

  return Report();
}
