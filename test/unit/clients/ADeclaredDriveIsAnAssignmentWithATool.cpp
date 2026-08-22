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
  <drive fromLat="48.1371" fromLon="11.5754" toLat="53.5503" toLon="9.9920" zoom="10"/>
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
  CHECK(declared.Driven.Declared && declared.Driven.Zoom == 10,
        "the drive is two coordinates and a zoom, nothing more");

  Store scene;
  outshine::Column<outshine::Vehicle> vehicles;
  outshine::Column<outshine::Drive> drives;
  CHECK(scene.Open(8) && vehicles.Open(scene) && drives.Open(scene), "the graph opens");
  Assembled stood;
  const bool assembled = Assemble(declared, scene, vehicles, drives, stood, error);
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
  CHECK(lone.Open(8) && loneV.Open(lone) && loneD.Open(lone), "a second graph opens");
  Assembled nobody;
  CHECK(!Assemble(mindless, lone, loneV, loneD, nobody, error),
        "**A DRIVE WITHOUT A MIND IS REFUSED AT ASSEMBLY**: somebody must take the assignment, "
        "and the refusal says to declare a player");

  Covers("II.9 a declared drive assembles as the actor chain end to end: mind Uses a nav tool, "
         "mind Assigned an assignment carrying the two coordinates, ordered by the rules and "
         "refused without a mind");
  return Report();
}
