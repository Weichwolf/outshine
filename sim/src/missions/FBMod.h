/* WHERE THE CONTENT IS. The engine owns no aircraft, no mesh and no mission: they belong to a mod, and
 * mod.json is the only place their directories are named (doc/mods.md §3). Everything that used to
 * hard-code `assets/aircraft` or `missions/` asks this instead.
 *
 * Smallest form that carries: ONE mod, no registry, no capability negotiation. The default directory is
 * the only name of a mod left in the engine, and it lives here so a grep finds exactly one. */
#ifndef FBMOD_H
#define FBMOD_H

#include "FBModelRoots.h"
#include <string>

namespace FlightBox::Missions {

struct FBMod {
  std::string Dir, Id, Name;
  std::string Aircraft, Models, Missions, Campaigns, Data;   /* resolved against Dir */

  FBModelRoots Roots() const { return FBModelRoots{Aircraft, Data}; }
  /* A `.fbm`/`.fbc` suffix means the caller brought a PATH; anything else is a name in the mod. The
   * rule is a suffix and not a stat() so the same argument resolves the same way on every machine. */
  std::string Mission(const std::string &nameOrPath) const;
  std::string Campaign(const std::string &nameOrPath) const;
};

bool FBLoadMod(const std::string &dir, FBMod &out, std::string *err);

/* Relative to the client's working directory, which for every target in this tree is sim/. */
inline const char *FBDefaultModDir() { return "../mods/f16"; }

/* For callers with no CLI to carry a --mod: the harnesses. A failed load leaves the roots empty, and
 * the loader that gets them says which file it could not open — a second error path would only bury it. */
inline FBMod FBDefaultMod() {
  FBMod m;
  FBLoadMod(FBDefaultModDir(), m, nullptr);
  return m;
}

} // namespace FlightBox::Missions
#endif
