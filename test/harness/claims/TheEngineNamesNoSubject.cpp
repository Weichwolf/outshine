#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include <cstdlib>

#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

// AN ENGINE KNOWS LAWS AND NO SUBJECTS, and this counts the words that break it.
//
// Unreal keeps `FBodyInstance`, `FConstraintInstance` and the solver in the engine module, and
// wheeled movement -- `UChaosVehicleMovementComponent`, its wheels, its engine curve -- in a
// PLUGIN outside it. RAGE keeps `phInst` and `phConstraint` in physics and `CVehicle`, `CWheel`
// and `CSeatManager` in the game layer above `fwEntity`. Both agree, and they agree for a reason
// that outlives either engine: a noun like `wheel` decides the shape of everything downstream of
// it. A wheel implies an axle, an axle implies a pair, a pair implies symmetry -- and a machine
// with one driven wheel, or three, or a track, cannot be said at all.
//
// The engine's vocabulary is the LAW: body, joint, degree of freedom, drive, constraint, force,
// contact, integration. A car, a wheel, a seat, a door or a walker is an ASSEMBLY a scenario
// builds out of those, and it is named in the scenario, in the client, or in a generator -- never
// in `src/` or `include/`.
//
// THE WALK MATCHES AN IDENTIFIER COMPONENT, NOT A BARE WORD, and board:2079 is what that cost.
// The first version of this claim grepped `\bWord\b`, and C++ does not name things in bare words:
// a word boundary after `Road` fails on `RoadStation`, so the claim read ZERO while `src/engine`
// named roads thirty-two times. `Terrain` declared zero against two hundred and fourteen, with a
// comment beside the row asserting the rule held. A green claim is read as evidence, so a blind
// one is worse than none.
//
// The pattern is `Word([^a-z]|$)`: the word followed by anything that is not a lower-case letter.
// `RoadStation` and `kRoadAboveM` match; `Carriageway` and `Cartesian` do not, which a plain
// substring walk would have swept up instead. Case matters, so a local called `road` is left to
// the reader the way `Engine` always was.
//
// TWO ZONES, because one number could not decide anything. `src/engine` and `include/` are the
// MOTOR and the DOOR and they must hold ZERO -- that rule has stood for weeks and both references
// keep it: Unreal's engine module has no road, RAGE's `fwEntity` has no bridge. Everywhere else
// carries a DECLARED count that may only fall, which is where a data tier's `TerrainGrid` and a
// path's `Carriageway` live while they are decided.
//
// WHAT IS DELIBERATELY NOT ON THE LIST, because a blacklist that cries wolf gets ignored:
//   Drive     CLAUDE.md's own vocabulary -- EFFORT or MOTION on a degree of freedom. A drive is
//             a law. `DriveTick` and `DriveAssembly` are named for it, not for a car.
//   Bus       an audio bus, which is Unreal's submix and RAGE's mixer bus. A law of routing.
//   Engine    this engine. Only an internal-combustion `engine` would be a subject, and the
//             walk cannot tell them apart, so the word is left to the reader.
//   Body      the law itself -- Unreal's FBodyInstance, RAGE's phInst.
//   Contact   the law -- where two bodies touch. `Tyre` is the subject that sits on one.
//
// AND `src/generators/` IS EXEMPT, which is not a loophole but the point. A generator's whole
// job is to MAKE one concrete thing, so a tree grower called `Tree` is named for what it
// produces and could not be named anything else without lying. CLAUDE.md names them that way
// itself -- forest, buildings, water, infrastructure -- and the tier links without the engine
// behind it, so a subject noun there decides nothing about the laws. The engine hands back
// geometry; what the geometry IS belongs to whoever grew it. Unreal draws the same line: PCG
// and its point-generation nodes sit outside the physics and renderer modules that must stay
// generic.

namespace {

struct Forbidden {
  const char *Word;
  size_t Standing;
  const char *Why;
};

constexpr const char *kMotor = "src/engine include";
constexpr const char *kRest = "src --exclude-dir=generators --exclude-dir=engine";

size_t Named(const char *word, const char *where) {
  const std::string counted =
      outshine::Test::Ask(std::string("grep -rohE '") + word + "([^a-z]|$)' " + where +
                          " --include='*.h' --include='*.cpp' 2>/dev/null | wc -l | tr -d ' '");
  return counted.empty() ? 0u : (size_t)std::strtoul(counted.c_str(), nullptr, 10);
}

// The count is what the REST held when the walk was fixed. The motor's own count is always zero.
constexpr Forbidden kSubjects[] = {
    {"Car", 12, "the assembly itself; RAGE's CVehicle lives in the game layer"},
    {"Seat", 39, "RAGE's CSeatManager is game-layer; the law is a claimable SLOT with a state"},
    {"Door", 17, "an assembly of a body and a revolute joint with a drive"},
    {"Steering", 8, "a lever ratio on a drive -- a ratio in the same statement, not a part"},
    {"Brake", 24, "a drive that may not add energy -- the same law as a motor, one sign apart"},
    {"Throttle", 2, "a control command over time, not a part"},
    {"Tyre", 0, "moved onto Contact by board:1897 and must not come back"},
    {"Wheel", 2, "the noun that implies an axle, a pair and a symmetry"},
    {"Chassis", 0, "a body"},
    {"Axle", 0, "a joint between two bodies"},
    {"Pedal", 0, "a control command over time"},
    {"Forest", 0, "a generator's subject; the engine holds a Making and never what it depicts"},
    {"Terrain",
     214,
     "TerrainGrid 69, TerrainField 40, TerrainMesh 34, TerrainBytes 31, TerrainTiles 24, and the "
     "rest -- every one in the DATA tier and none in the motor. Whether a height field is a law or "
     "a subject is board:2079's to decide; what is settled is that the engine does not say it"},
    {"Building", 17, "the house is BuildingField in the data tier and BuildingMesh in a generator"},
    {"Road", 8, "a corridor is a reference line; a road is what OSM calls one"},
    {"River", 0, "a ribbon with a width; what it depicts is the generator's business"},
    {"Mountain", 0, "ground is a height field and a slope, and it has no proper nouns"},
    {"Tree", 18, "a data structure, and the world's tree belongs to the generators"},
    {"Walker", 0, "an assembly; walking is CONTROL over time"},
    {"Street", 30, "OSM's word for a road, and it decides nothing about a law"},
    {"Bridge", 4, "what a deck DOES is span; a bridge is what a map calls the result"},
    {"Tunnel", 1, "the same, one sign apart"},
    {"Kerb", 5, "a profile's edge, which is a number in a cross-section"},
    {"Carriageway",
     12,
     "the surface a corridor sweeps, named for what drives on it. Two of the thirteen left with "
     "Ribbon and Fit when board:2083 moved the makers into the generators, where a subject noun "
     "is exempt by design"},
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  size_t rose = 0, fell = 0, inMotor = 0;
  std::printf("  %-12s %7s %7s %7s\n", "word", "motor", "rest", "owed");
  for (const Forbidden &one : kSubjects) {
    const size_t motor = Named(one.Word, kMotor);
    const size_t rest = Named(one.Word, kRest);
    inMotor += motor;
    if (rest > one.Standing) { ++rose; }
    if (rest < one.Standing) { ++fell; }
    const char *moved =
        motor > 0 ? "  IN THE MOTOR"
                  : (rest > one.Standing ? "  ROSE" : (rest < one.Standing ? "  fell" : ""));
    std::printf("  %-12s %7zu %7zu %7zu%s\n", one.Word, motor, rest, one.Standing, moved);
    if (motor > 0 || rest != one.Standing) { std::printf("               %s\n", one.Why); }
  }
  std::printf("\n  %zu subject noun(s) in src/engine and include/, where the count is ZERO\n",
              inMotor);

  CHECK(inMotor == 0,
        "**THE MOTOR AND THE DOOR NAME NO SUBJECT**: outshine's engine may not know a street, a "
        "bridge or a house -- its vocabulary is body, mesh, material, instance, tile, and what a "
        "thing IS belongs to whoever generated it. Unreal's engine module has no road and RAGE's "
        "fwEntity has no bridge. This is RED by measurement rather than by aspiration: the walk "
        "that reported zero could not see a compound, and board:2078 moves the derivation out of "
        "src/engine/Laying.cpp, which holds almost all of it");

  CHECK(rose == 0,
        "**OUTSIDE THE MOTOR THE COUNT MAY ONLY FALL**: a subject noun decides the shape of "
        "everything downstream -- a wheel implies an axle, an axle implies a pair, and a machine "
        "with one driven wheel or a track cannot then be said at all. Unreal puts wheeled movement "
        "in a plugin and RAGE puts CVehicle in the game layer; both keep the engine speaking laws");
  CHECK(fell == 0,
        "**A COUNT THAT FELL IS RECORDED WHERE IT FELL**: this is not a failure, it is the claim "
        "asking for its own number to be updated in the commit that removed the word, so the "
        "next reader sees what the tree holds rather than what it once held");

  Covers("the engine's vocabulary: `src/engine` and `include/` name NO subject -- car, seat, door, "
         "steering, brake, throttle, tyre, wheel, chassis, axle, pedal, walker, street, road, "
         "bridge, tunnel, kerb, carriageway, building, terrain -- and outside them the count may "
         "only fall. The walk matches an identifier COMPONENT, so a compound cannot hide a word");
  return Report();
}
