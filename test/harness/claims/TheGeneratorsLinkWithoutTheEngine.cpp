#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Lines;
using outshine::Test::Run;

namespace {

// THE OWNER'S TARGET, CHECKED BY THE LINKER. outshine ships a generator registry, a client adds
// its own, and ANOTHER PROJECT USES THE GENERATORS ALONE (board:1948). The third is the binding
// one, and it is not a matter of taste or of includes: either the generator objects resolve their
// undefined symbols without the engine's, or a foreign program cannot link them.
//
// `test/harness/shared/frame/linkreach.sh` computes the closure the linker itself computes when
// it pulls members out of a `.a`: every object reachable from a seed by UNDEFINED SYMBOL. This
// walk is SOUND where the frame path's relocation walk is not -- a virtual call carries no symbol
// at its call site, but the vtable naming its overrides is a relocation in the referencing
// object, so the override is undefined there and the closure crosses it.
//
// Measured when this claim was written: 49 objects, 42 of them `world` and 7 `base`, and nothing
// from `engine`, `render`, `scenario`, `sim`, `ui`, `audio` or `host`. The tier was already
// separable and nothing said so, which is how a separation is lost -- one include at a time,
// with the gate green throughout.
constexpr const char *kSeed = "src-world-generators";

// A tier the generators may reach, with the reason. `world` is their own and `base` reaches
// nothing, so both are safe for a foreign program to take. Everything else is outshine's own
// program, and a library that drags it in is not a library.
constexpr const char *kMayReach[] = {"src-world-", "src-base-"};

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this claim unpacks the archive into the runner's nest and was given none");
    return Report();
  }

  std::string newer;
  (void)Run("find src include -newer build/liboutshine.a -name '*.cpp' -o -newer "
            "build/liboutshine.a -name '*.h' 2>/dev/null | head -3",
            newer);
  if (!newer.empty()) {
    std::printf("%s", newer.c_str());
    Unprepared("a source is newer than build/liboutshine.a, so this walk would judge a link the "
               "tree has already left -- run make");
    return Report();
  }

  std::string reached;
  const int walked = Run("sh test/harness/shared/frame/linkreach.sh build/liboutshine.a " +
                             std::string(nest) + "/linkwalk " + kSeed + " 2>/dev/null",
                         reached);
  CHECK(walked == 0 && !reached.empty(),
        "the archive walks -- a claim that cannot read the link it judges is UNPREPARED, never "
        "green");
  if (walked != 0 || reached.empty()) { return Report(); }

  std::vector<std::string> dragged;
  size_t objects = 0, seeded = 0;
  for (const std::string &line : Lines(reached)) {
    if (line.empty()) { continue; }
    ++objects;
    if (line.compare(0, std::string(kSeed).size(), kSeed) == 0) { ++seeded; }
    bool allowed = false;
    for (const char *prefix : kMayReach) {
      if (line.compare(0, std::string(prefix).size(), prefix) == 0) { allowed = true; }
    }
    if (!allowed) { dragged.push_back(line); }
  }

  std::printf("SEEDED %zu generator object(s), REACHED %zu in all\n", seeded, objects);
  for (const std::string &one : dragged) { std::printf("  DRAGS IN  %s\n", one.c_str()); }

  CHECK(seeded > 10,
        "the seed matched the generator objects it names -- a seed that matched two of them would "
        "report a clean link for the same reason an empty walk does");
  CHECK(objects > seeded,
        "and the closure reached PAST the seed, so it is resolving symbols rather than listing "
        "the files it started from");
  CHECK(dragged.empty(),
        "**THE GENERATORS LINK WITHOUT THE ENGINE**: every object their symbols pull in is `world` "
        "or `base`, so another project takes the generator tier and nothing of outshine's own "
        "program comes with it. That is the owner's third sentence and the one the other two "
        "follow from -- a tier that must stand up in a foreign program cannot name the renderer, "
        "the scenario, the sim or the engine");

  // AND THE ARCHIVE THAT PROVES IT BY CONSTRUCTION. `make` writes `build/libgenerators.a` from the
  // same closure this claim walks -- a derived member list, never a second one kept by hand -- and
  // this program links against it and NOTHING else. A claim that the tier WOULD link is weaker
  // than a program that does.
  std::string built;
  const std::string alone = std::string(nest) + "/generators-alone";
  const int made =
      Run("c++ -std=c++23 -Wall -Werror -Wpedantic -Iinclude -Isrc/base/math -Isrc/base/geo "
          "-Isrc/base/format -Isrc/base/spatial -Isrc/base/io -Isrc/content/shade "
          "-Isrc/content/gltf -Isrc/world/sky -Isrc/world/weather -Isrc/world/ground "
          "-Isrc/generators -Isrc/world/data test/harness/shared/frame/GeneratorsAlone.cpp "
          "build/libgenerators.a -o " + alone + " 2>&1",
          built);
  if (made != 0) { std::printf("%s", built.c_str()); }
  CHECK(made == 0,
        "**A PROGRAM LINKS THE GENERATOR ARCHIVE AND NOTHING ELSE**: not `liboutshine.a`, not "
        "SDL, not a single object of outshine's own program. That is the owner's third sentence "
        "-- another project uses the generators alone -- and an archive that links is worth more "
        "than a walk that says it would");
  if (made == 0) {
    std::string said;
    const int ran = Run(alone, said);
    std::printf("ALONE  %s", said.c_str());
    CHECK(ran == 0 && said.find("built") != std::string::npos,
          "and it RUNS: a ground table from rows and a height patch from postings, both value "
          "factories a foreign caller can reach with no world, no file and no engine behind them");
  }

  Covers("the generator library: the objects its symbols reach are `world` and `base` alone, so "
         "the tier links into a foreign program without dragging outshine's engine, renderer, "
         "scenario or sim behind it -- walked as the LINKER walks it, over undefined symbols, and "
         "proven again by a program that links `build/libgenerators.a` alone and runs");
  return Report();
}
