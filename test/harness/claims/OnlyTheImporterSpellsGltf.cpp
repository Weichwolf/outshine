#include <cstdio>
#include <cstdlib>
#include <string>

#include "Check.h"
#include "Shell.h"

// GLTF IS A DOOR, AND A DOOR IS A PLACE RATHER THAN A LANGUAGE.
//
// Unreal's glTF importer ends in the editor: it produces the same cooked asset a native one does,
// and nothing at runtime carries a glTF type. RAGE's importers land in `grmGeometry` and the format
// a thing arrived in is gone by the time anything draws it. Both agree, and neither carries an
// interchange format inward -- so the matter is closed and this claim holds the line.
//
// WHY IT IS A LINE AND NOT A PREFERENCE, measured in this tree: `Gltf::Subject` was the value the
// world path crossed on, and because a glTF stores doubles, the generators' float positions were
// WIDENED on the way in and NARROWED again on the way to the device. float -> double -> float over
// 28 M vertices, for a buffer that is float either way. That cost 2 437 ms of assembly and 2 708 ms
// of packing on Shibuya and doubled the footprint in between. A file format's storage decision,
// applied to geometry that never came from a file.
//
// WHAT THIS CLAIM DOES NOT DECIDE. It counts a SPELLING, not a dependency: a tier may still read a
// glTF file through the importer's own door, and this says nothing about whether it should. It also
// cannot see a glTF concept wearing another name -- an axis convention, a double where a float
// belongs -- so a green here is not a promise that the format stopped travelling, only that its
// namespace did.
//
// THE NUMBERS ARE WHAT STOOD WHEN THIS WAS WRITTEN. They may FALL and never rise, which is the same
// instrument `TheRendererNamesNoSubject` uses and for the same reason: board:1547 recorded ten
// conversion sites, waited, and found fourteen -- "which is what an unguarded count does".

namespace {

struct Tier {
  const char *Where;
  size_t Standing;
};

constexpr Tier kTiers[] = {
    {"src/engine", 95}, {"src/render", 34}, {"src/generators", 6}, {"src/base", 1},
    {"src/scene", 0},   {"src/scenario", 0}, {"src/sim", 0},       {"src/world", 0},
    {"src/compositor", 0}, {"src/ui", 0},   {"src/host", 0},       {"src/audio", 0},
    {"include", 0},     {"apps", 0},
};

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  size_t rose = 0;
  size_t whole = 0;
  for (const Tier &one : kTiers) {
    const std::string counted =
        Ask(std::string("grep -roh 'Gltf::' ") + one.Where + " 2>/dev/null | wc -l | tr -d ' '");
    const size_t now = counted.empty() ? 0u : (size_t)std::strtoul(counted.c_str(), nullptr, 10);
    whole += now;
    if (now != one.Standing) {
      std::printf("  %-16s %zu, declared %zu\n", one.Where, now, one.Standing);
      if (now > one.Standing) { ++rose; }
    }
  }
  std::printf("  %zu spellings of Gltf:: stand outside src/content/gltf/\n", whole);

  CHECK(rose == 0,
        "**ONLY THE IMPORTER SPELLS GLTF**: an interchange format is a door rather than an inward "
        "language. Unreal's glTF importer ends in the editor and RAGE's importers end in "
        "grmGeometry; past either, nothing knows what file a thing came from. A tier that spells "
        "Gltf:: has that format's decisions -- its axes, its storage widths -- reaching into work "
        "that never involved a file, and this count may fall but never rise");

  Covers("where the glTF namespace is spelled: outside src/content/gltf/ the count may only fall, "
         "so the importer's own vocabulary stops at its door -- it does NOT judge whether a glTF "
         "concept travels inward under another name");
  return Report();
}
