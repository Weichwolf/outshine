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
// THE COUNT IS DECLARED AND MAY ONLY FALL, which is the same instrument as `--audit-access` and
// `EXPECT_FAIL`. A claim demanding zero today would be a claim nobody could turn green, so it
// would be turned off instead. A declared count refuses in BOTH directions: every word that
// leaves does so in a commit that says why, and every word that arrives does too.
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

// Each count is what the tree held when the claim was written. They may only fall.
constexpr Forbidden kSubjects[] = {
    {"Car", 6, "the assembly itself; RAGE's CVehicle lives in the game layer"},
    {"Seat", 16, "RAGE's CSeatManager is game-layer; the law is a claimable SLOT with a state"},
    {"Door", 13, "an assembly of a body and a revolute joint with a drive"},
    {"Steering", 8, "a lever ratio on a drive -- a ratio in the same statement, not a part"},
    {"Brake", 4, "a drive that may not add energy -- the same law as a motor, one sign apart"},
    {"Throttle", 2, "a control command over time, not a part"},
    {"Tyre", 0, "moved onto Contact by board:1897 and must not come back"},
    {"Wheel", 0, "the noun that implies an axle, a pair and a symmetry"},
    {"Chassis", 0, "a body"},
    {"Axle", 0, "a joint between two bodies"},
    {"Pedal", 0, "a control command over time"},
    {"Walker", 0, "an assembly; walking is CONTROL over time"},
};

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  size_t rose = 0, fell = 0;
  std::printf("  %-10s %6s %6s\n", "word", "now", "owed");
  for (const Forbidden &one : kSubjects) {
    const std::string counted =
        Ask(std::string("grep -rowh '\\b") + one.Word +
            "' src/ include/ --include='*.h' --include='*.cpp' "
            "--exclude-dir=generators 2>/dev/null | wc -l | tr -d ' '");
    const size_t now = counted.empty() ? 0u : (size_t)std::strtoul(counted.c_str(), nullptr, 10);
    const char *moved = now > one.Standing ? "  ROSE" : (now < one.Standing ? "  fell" : "");
    if (now > one.Standing) { ++rose; }
    if (now < one.Standing) { ++fell; }
    std::printf("  %-10s %6zu %6zu%s%s\n", one.Word, now, one.Standing, moved,
                now != one.Standing ? "" : "");
    if (now != one.Standing) { std::printf("             %s\n", one.Why); }
  }

  CHECK(rose == 0,
        "**THE ENGINE NAMES NO SUBJECT, AND THE COUNT MAY ONLY FALL**: a subject noun in `src/` "
        "or `include/` decides the shape of everything downstream -- a wheel implies an axle, an "
        "axle implies a pair, and a machine with one driven wheel or a track cannot then be said "
        "at all. Unreal puts wheeled movement in a plugin and RAGE puts CVehicle in the game "
        "layer; both keep the engine speaking laws");
  CHECK(fell == 0,
        "**A COUNT THAT FELL IS RECORDED WHERE IT FELL**: this is not a failure, it is the claim "
        "asking for its own number to be updated in the commit that removed the word, so the "
        "next reader sees what the tree holds rather than what it once held");

  Covers("the engine's vocabulary: no subject noun -- car, seat, door, steering, brake, throttle, "
         "tyre, wheel, chassis, axle, pedal, walker -- grows in `src/` or `include/`, and every "
         "one that leaves is recorded in the commit that removed it");
  return Report();
}
