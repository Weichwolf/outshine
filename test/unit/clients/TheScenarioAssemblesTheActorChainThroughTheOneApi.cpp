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
namespace tags = outshine::tags;

namespace {

bool Slurp(const char *path, std::string &into) {
  std::FILE *file = std::fopen(path, "rb");
  if (file == nullptr) { return false; }
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    into.append(block, read);
  }
  std::fclose(file);
  return true;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string text;
  std::string error;
  Scenario declared;
  CHECK(Slurp("apps/driver/f31.scenario", text) &&
            ReadScenario(text.c_str(), text.size(), declared, error),
        "the driver's own scenario reads -- the same file the game declares itself with");

  Store scene;
  CHECK(scene.Open(16), "a store opens for the handful the scenario declares");
  outshine::Column<outshine::Vehicle> vehicles;
  CHECK(vehicles.Open(scene), "and a column for the vehicles' numbers beside it");
  outshine::Column<outshine::Drive> drives;
  CHECK(drives.Open(scene), "and one for the drives");
  outshine::Column<outshine::Traits> kinds;
  CHECK(kinds.Open(scene), "and one for the kinds' resolved traits");
  Assembled stood;
  const bool assembled = Assemble(declared, scene, vehicles, drives, kinds, stood, error);
  if (!assembled) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(assembled, "**THE XML DOOR IS THE SAME DOOR**: the scenario declaration assembles through "
                   "Store::Add, Give and Link -- the calls a C++ client makes -- against the one "
                   "graph, so there is no second representation to drift (the parity law of "
                   "board:1583)");

  CHECK(stood.Bodies.size() == 1 && scene.RoleOf(stood.Bodies[0]) == Role::Body,
        "one declared vehicle is one body");
  CHECK(scene.Has(stood.Bodies[0], tags::DoesSteer) && scene.Has(stood.Bodies[0], tags::DoesDrive) &&
            scene.Has(stood.Bodies[0], tags::DoesBrake),
        "**ITS FUNCTIONS ARE DERIVED FROM THE DECLARATION, NEVER INVENTED**: a turning circle is "
        "steering, torque through a final drive is drive, brake torque is brake -- each tag "
        "stands because a physical quantity stands in the file");
  const outshine::Vehicle *carried = vehicles.Get(stood.Bodies[0]);
  CHECK(carried != nullptr && carried->MassKg == 1610.0 && carried->Contacts.size() == 4,
        "**THE NUMBERS RIDE ON THE ENTITY**: the declaration's mass and its four contacts read "
        "back through the generation-checked handle, so a system that holds the handle holds "
        "the data -- no second lookup by name");
  CHECK(!scene.Has(stood.Bodies[0], tags::DoesLamp),
        "and what the file does not declare, the body does not do: no lamp is spelled, so no "
        "lamp tag is given");

  CHECK(scene.Alive(stood.PlayerMind) &&
            scene.TargetOf(stood.PlayerBody, Relation::DrivenBy) == stood.PlayerMind,
        "**THE PLAYER IS A MIND AT THE ONE SEAM**: <player is=\"f31\"> becomes a Mind entity "
        "linked DrivenBy to the declared body -- the seam an autopilot uses, board:1581's chain");

  const Entity rival = scene.Add(Role::Mind);
  CHECK(!scene.Link(stood.PlayerBody, Relation::DrivenBy, rival),
        "**THE GRAPH THE XML BUILT OBEYS THE CODE DOOR'S RULES**: a rival mind is refused the "
        "seat the declaration filled, by the exclusivity trait on the relation itself");

  Store crowded;
  CHECK(crowded.Open(1), "a store of one seat is a store");
  outshine::Column<outshine::Vehicle> few;
  CHECK(few.Open(crowded), "with a column to match");
  outshine::Column<outshine::Drive> fewDrives;
  CHECK(fewDrives.Open(crowded), "and a drive column beside it");
  outshine::Column<outshine::Traits> fewKinds;
  CHECK(fewKinds.Open(crowded), "and a traits column");
  Assembled cramped;
  CHECK(!Assemble(declared, crowded, few, fewDrives, fewKinds, cramped, error),
        "a declaration that does not fit is refused at assembly, not truncated");
  const std::string doorText = error;
  (void)crowded.Add(Role::Mind);
  CHECK(crowded.Error() == doorText,
        "**AND THE REFUSAL TEXT IS THE SAME FROM BOTH DOORS**, because there is one checker: the "
        "store spoke, not a parser and not a client");

  Covers("II.4 the scenario XML assembles the actor chain through the same store API a C++ "
         "client calls: one graph, two doors, one checker, identical refusal text");
  return Report();
}
