/* WHERE THE CONTENT IS. The engine owns no aircraft, no mesh and no mission: they belong to a mod, and
 * mod.json is the only place their directories are named (doc/mods.md §3). Everything that used to
 * hard-code `assets/aircraft` or `missions/` asks this instead.
 *
 * Smallest form that carries: no registry, no capability negotiation. The default directory is
 * the only name of a mod left in the engine, and it lives here so a grep finds exactly one. */
#ifndef FBMOD_H
#define FBMOD_H

#include "FBModelRoots.h"
#include <string>
#include <vector>

namespace FlightBox::Missions {

struct FBMod {
  std::string Dir, Id, Name;
  std::string Aircraft, Models, Missions, Campaigns, Data;   /* resolved against Dir */
  /* MODULE REGISTRY KEYS that ship a mesh under Models — `<key>.asset.json` plus the `.glb` levels it
   * names (render/FBUnitModel.h). A key and not a filename, because the key is what a unit publishes
   * as its visual type; a client that hard-coded one could only ever draw one mod's airframes. */
  std::vector<std::string> Meshes;
  /* OTHER MOD IDS, searched in order for a root this manifest does not name — a sibling under the same
   * `mods/`. One level, no transitivity: a title that borrows another's airframes says so, and a
   * dependency that borrows further is a graph nothing here needs. */
  std::vector<std::string> Depends;
  std::string Sandbox;          /* module registry key a client's no-mission sandbox spawns */
  std::string DefaultMission;   /* what a client flies when the player named none */

  FBModelRoots Roots() const { return FBModelRoots{Aircraft, Data}; }
  /* A `.fbm`/`.fbc` suffix means the caller brought a PATH; anything else is a name in the mod. The
   * rule is a suffix and not a stat() so the same argument resolves the same way on every machine. */
  std::string Mission(const std::string &nameOrPath) const;
  std::string Campaign(const std::string &nameOrPath) const;
};

/* TWO MOUNTS OF ONE MOD. `root` is what the relative directories resolve against, `dir` is where the
 * manifest is read from — the same everywhere except in the browser, which reads mod.json out of the
 * preloaded filesystem and fetches the SAME relative directories over HTTP from a different root. */
bool FBLoadMod(const std::string &dir, const std::string &root, FBMod &out, std::string *err);
inline bool FBLoadMod(const std::string &dir, FBMod &out, std::string *err) {
  return FBLoadMod(dir, dir, out, err);
}

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
