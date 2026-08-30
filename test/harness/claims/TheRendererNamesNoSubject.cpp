#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;

// THE RENDERER KNOWS NO SUBJECT, and this is stricter than the tree-wide rule beside it.
//
// Unreal draws Landscape, foliage, water and a character through ONE route: a
// UPrimitiveComponent with an FPrimitiveSceneProxy in FScene, gathered by the base pass. There is
// no terrain pass. RAGE puts terrain, props and vehicles on one draw list as map entities. Both
// agree and neither has an exception, because what a thing DEPICTS is decided above the renderer
// -- a generator's business, never a pass's.
//
// This tree had four passes named after things: `Stage::Terrain`, `Stage::Buildings`,
// `Stage::Water` and `Stage::Models`, each declaring resource edges and executing NOTHING. A
// declaration surface with a subject's name on it, which is two findings rather than one.
//
// WHY A SECOND CLAIM RATHER THAN A LONGER LIST IN THE FIRST. `TheEngineNamesNoSubject` walks
// `src/` and `include/` for the vocabulary of a MACHINE -- car, wheel, seat, steering. It did not
// catch these, because `terrain` is not a machine part and had no reason to be on that list. The
// renderer's forbidden words are different: they are the things a WORLD is made of. A guard is
// only as good as the words it knows, so the renderer gets its own.
//
// `src/generators/` is exempt from the other claim because a generator's job is to make one
// concrete thing. `src/render/` gets no such exemption: it is the tier whose whole purpose is
// not to care what it is drawing.

namespace {

struct Forbidden {
  const char *Word;
  size_t Standing;
};

// What `src/render/` held when this claim was written -- zero, and it must stay zero.
constexpr Forbidden kSubjects[] = {
    {"Terrain", 0},
    {"Buildings", 0},
    {"Building", 0},
    {"Water", 0},
    {"Models", 0},
    {"Forest", 0},
    {"Tree", 0},
    {"Road", 0},
    {"Grass", 0},
    {"Cloud", 0},
    {"Ocean", 0},
    {"River", 0},
    {"Car", 0},
    {"Vehicle", 0},
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  size_t rose = 0;
  for (const Forbidden &one : kSubjects) {
    const std::string counted = Ask(std::string("grep -rowh '") + one.Word +
                                    "' src/render/ 2>/dev/null | wc -l | tr -d ' '");
    const size_t now = counted.empty() ? 0u : (size_t)std::strtoul(counted.c_str(), nullptr, 10);
    if (now != one.Standing) {
      std::printf("  %-12s %zu, declared %zu\n", one.Word, now, one.Standing);
      if (now > one.Standing) { ++rose; }
    }
  }
  if (rose == 0) {
    std::printf("  src/render/ names none of the %zu\n", sizeof kSubjects / sizeof kSubjects[0]);
  }

  CHECK(rose == 0,
        "**THE RENDERER KNOWS NO SUBJECT**: one route from data to pixel, and what a thing "
        "depicts is decided above the renderer. Unreal draws Landscape as a primitive in the base "
        "pass and RAGE puts terrain on the same draw list as everything else -- neither has a "
        "pass named after a thing, because a pass named after a thing is a second route waiting "
        "to be written");

  Covers("the render tier's vocabulary: no name in `src/render/` says what is being drawn, so "
         "there is one route from a cooked geometry to a pixel and no second one for terrain");
  return Report();
}
