#include <cstdio>
#include <cstring>
#include <string>

#include "Check.h"

#include "Assembly.h"
#include "ScenarioRead.h"

using outshine::Assembled;
using outshine::Assemble;
using outshine::AssembledCapacity;
using outshine::Column;
using outshine::Drive;
using outshine::Entity;
using outshine::kNoEntity;
using outshine::ReadScenario;
using outshine::Relation;
using outshine::Scenario;
using outshine::Store;
using outshine::Traits;
using outshine::Vehicle;
namespace tags = outshine::tags;

namespace {

const char *kDeclared = R"(<scenario name="nouns">
  <kinds>
    <kind name="vessel"><has name="volumeL" value="0.5"/><has name="massKg" value="0.4"/></kind>
    <kind name="mug" inherits="vessel"><may do="lamp"/><has name="volumeL" value="0.3"/></kind>
  </kinds>
  <instances>
    <instance of="mug" id="cup"><has name="massKg" value="0.35"/></instance>
    <instance of="vessel" id="crate"><holds what="cup"/></instance>
    <instance of="mug" id="spare" in="crate"/>
  </instances>
</scenario>)";

bool Stand(const char *text, Scenario &declared, Store &scene, Column<Vehicle> &vehicles,
           Column<Drive> &drives, Column<Traits> &kinds, Assembled &stood, std::string &error) {
  if (!ReadScenario(text, std::strlen(text), declared, error)) { return false; }
  return scene.Open(AssembledCapacity(declared) + 4) && vehicles.Open(scene) &&
         drives.Open(scene) && kinds.Open(scene) &&
         Assemble(declared, scene, vehicles, drives, kinds, stood, error);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;
  Scenario declared;
  Store scene;
  Column<Vehicle> vehicles;
  Column<Drive> drives;
  Column<Traits> kinds;
  Assembled stood;
  const bool up = Stand(kDeclared, declared, scene, vehicles, drives, kinds, stood, error);
  if (!up) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(up, "two kinds and three instances stand through the one assembly door");
  if (!up) { return Report(); }

  const Entity cup = stood.InstanceNamed("cup");
  const uint32_t volumeL = stood.TraitKey("volumeL");
  const uint32_t massKg = stood.TraitKey("massKg");
  const Traits *held = kinds.Get(cup);
  CHECK(held != nullptr && volumeL != 0 && massKg != 0, "the cup carries resolved traits");
  if (held == nullptr) { return Report(); }
  CHECK(held->Named(volumeL) != nullptr && *held->Named(volumeL) == 0.3,
        "**AN INSTANCE'S ATTRIBUTES ARE ITS KIND'S, OVERRIDDEN BY ITS OWN, RESOLVED ONCE**: "
        "the mug's 0.3 L overrode the vessel's 0.5 L at stand-up, and the read walked no "
        "chain to find it");
  CHECK(held->Named(massKg) != nullptr && *held->Named(massKg) == 0.35,
        "and the cup's own 0.35 kg overrode the vessel's 0.4 -- kind default, instance last "
        "word");
  CHECK(scene.Has(cup, tags::DoesLamp),
        "the capability flows down the inherits chain by IsA query, as the component model "
        "already proves");

  const Entity crate = stood.InstanceNamed("crate");
  const Entity spare = stood.InstanceNamed("spare");
  CHECK(scene.TargetOf(cup, Relation::HeldBy) == crate,
        "**HOLDING IS ONE MECHANISM AND ITS OWN RELATION**: the crate holds the cup by Holds, "
        "never by the subtree's ChildOf -- an inventory and a placement differ by one field, "
        "and instantiating a prefab cannot drag the world's contents with it");
  CHECK(scene.TargetOf(spare, Relation::HeldBy) == crate,
        "and standing IN something is the same relation from the other spelling");

  {
    Scenario bad;
    Store s;
    Column<Vehicle> v;
    Column<Drive> d;
    Column<Traits> k;
    Assembled a;
    CHECK(!Stand("<scenario name=\"t\"><kinds><kind name=\"m\" inherits=\"ghost\"/></kinds>"
                 "</scenario>",
                 bad, s, v, d, k, a, error) &&
              error.find("ghost") != std::string::npos,
          "inheriting what is not declared BEFORE it refuses naming it -- the order is the "
          "declaration's, and a cycle cannot even be spelled");
  }
  {
    Scenario bad;
    Store s;
    Column<Vehicle> v;
    Column<Drive> d;
    Column<Traits> k;
    Assembled a;
    CHECK(!Stand("<scenario name=\"t\"><kinds><kind name=\"m\"><may do=\"fly\"/></kind>"
                 "</kinds></scenario>",
                 bad, s, v, d, k, a, error) &&
              error.find("fly") != std::string::npos,
          "a capability the catalogue does not offer refuses naming it");
  }
  {
    Scenario bad;
    Store s;
    Column<Vehicle> v;
    Column<Drive> d;
    Column<Traits> k;
    Assembled a;
    CHECK(!Stand("<scenario name=\"t\"><kinds><kind name=\"m\">"
                 "<has name=\"h\" value=\"tall\"/></kind></kinds></scenario>",
                 bad, s, v, d, k, a, error) &&
              error.find("not a number") != std::string::npos,
          "**AN ATTRIBUTE IS A VALUE**: 'tall' refuses at assembly, because a tick carries "
          "no string");
  }

  {
    std::string deep = "<scenario name=\"t\"><kinds>";
    deep += "<kind name=\"k0\"/>";
    for (int at = 1; at < 9; ++at) {
      deep += "<kind name=\"k" + std::to_string(at) + "\" inherits=\"k" +
              std::to_string(at - 1) + "\"/>";
    }
    deep += "</kinds><instances><instance of=\"k8\" id=\"leaf\"/></instances></scenario>";
    Scenario bad;
    Store s;
    Column<Vehicle> v;
    Column<Drive> d;
    Column<Traits> k;
    Assembled a;
    CHECK(!Stand(deep.c_str(), bad, s, v, d, k, a, error) &&
              error.find("deeper than") != std::string::npos,
          "a kind chain deeper than the declared bound refuses rather than silently cutting "
          "the root's defaults");
  }
  {
    std::string wide = "<scenario name=\"t\"><kinds><kind name=\"base\">";
    for (int at = 0; at < 9; ++at) {
      wide += "<has name=\"a" + std::to_string(at) + "\" value=\"1\"/>";
    }
    wide += "</kind><kind name=\"leaf\" inherits=\"base\">";
    for (int at = 0; at < 8; ++at) {
      wide += "<has name=\"b" + std::to_string(at) + "\" value=\"2\"/>";
    }
    wide += "</kind></kinds><instances><instance of=\"leaf\" id=\"i\"/></instances></scenario>";
    Scenario bad;
    Store s;
    Column<Vehicle> v;
    Column<Drive> d;
    Column<Traits> k;
    Assembled a;
    CHECK(!Stand(wide.c_str(), bad, s, v, d, k, a, error) &&
              error.find("overflow") != std::string::npos,
          "and a chain whose key UNION overflows the trait budget refuses at the merge, "
          "never dropping a value on the floor");
  }

  Covers("III.5 a kind is a default and an instance overrides it: resolved once at stand-up "
         "into an interned-key column, inherits chains in declaration order, holding and "
         "standing-in are one HeldBy -- possession, never the subtree's ChildOf -- and every "
         "wrong spelling refuses by name "
         "(board:1487)");
  return Report();
}
