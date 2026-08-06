#include "FBMod.h"

#include <fstream>
#include <sstream>

namespace FlightBox::Missions {
namespace {

/* mod.json is a flat string map by construction, so a scanner for exactly that shape beats a JSON
 * library the tree would otherwise not need. Anything richer is a defect in the manifest, not a
 * missing feature here. */
bool Field(const std::string &text, const char *key, std::string &out) {
  const std::string needle = std::string("\"") + key + "\"";
  size_t k = text.find(needle);
  if (k == std::string::npos) return false;
  size_t c = text.find(':', k + needle.size());
  if (c == std::string::npos) return false;
  size_t a = text.find('"', c + 1);
  if (a == std::string::npos) return false;
  size_t b = text.find('"', a + 1);
  if (b == std::string::npos) return false;
  out = text.substr(a + 1, b - a - 1);
  return true;
}

/* The one list-shaped value: space-separated inside the flat map, so the scanner above stays the whole
 * parser and the Makefile's `sed` reads it with the same expression as every other field. */
void Words(const std::string &text, const char *key, std::vector<std::string> &out) {
  std::string flat;
  if (!Field(text, key, flat)) return;
  std::istringstream in(flat);
  for (std::string w; in >> w;) out.push_back(w);
}

bool Dir(const std::string &text, const std::string &root, const char *key, std::string &out) {
  std::string rel;
  if (!Field(text, key, rel)) return false;
  out = root + "/" + rel;
  return true;
}

const char *const kRootKeys[] = {"aircraft", "models", "missions", "campaigns", "data", "catalogue"};

std::string *RootSlot(FBMod &m, size_t i) {
  std::string *slots[] = {&m.Aircraft, &m.Models, &m.Missions, &m.Campaigns, &m.Data, &m.Catalogue};
  return slots[i];
}

/* A manifest read WITHOUT following its own `depends`, which is what keeps the search one level deep
 * and therefore free of a cycle guard: FBLoadMod composes the levels, so nothing here can call back. */
bool Manifest(const std::string &dir, const std::string &root, FBMod &out, std::string *err) {
  const std::string path = dir + "/mod.json";
  std::ifstream in(path);
  if (!in) { if (err) *err = "cannot open " + path; return false; }
  std::stringstream buf; buf << in.rdbuf();
  const std::string text = buf.str();

  out.Dir = dir;
  Field(text, "id", out.Id);
  Field(text, "name", out.Name);
  Field(text, "sandbox", out.Sandbox);
  Field(text, "default_mission", out.DefaultMission);
  Words(text, "meshes", out.Meshes);
  Words(text, "depends", out.Depends);
  for (size_t i = 0; i < sizeof(kRootKeys) / sizeof(*kRootKeys); ++i)
    Dir(text, root, kRootKeys[i], *RootSlot(out, i));
  Field(text, "dem", out.Dem);   /* still the bare filename here; Data may only arrive from `depends` */
  Dir(text, root, "hud", out.Hud);
  Dir(text, root, "hud_watch", out.HudWatch);
  return true;
}

} // namespace

std::string FBMod::Mission(const std::string &nameOrPath) const {
  if (nameOrPath.size() >= 4 && nameOrPath.compare(nameOrPath.size() - 4, 4, ".fbm") == 0)
    return nameOrPath;
  return Missions + "/" + nameOrPath + ".fbm";
}

std::string FBMod::Campaign(const std::string &nameOrPath) const {
  if (nameOrPath.size() >= 4 && nameOrPath.compare(nameOrPath.size() - 4, 4, ".fbc") == 0)
    return nameOrPath;
  return Campaigns + "/" + nameOrPath + ".fbc";
}

bool FBLoadMod(const std::string &dir, const std::string &root, FBMod &out, std::string *err) {
  if (!Manifest(dir, root, out, err)) return false;

  /* A ROOT THIS MANIFEST DOES NOT NAME IS THE DEPENDENCY'S — the whole of `depends`, and the reason a
   * mod that ships only missions does not have to copy another's airframes to fly them. Sibling
   * lookup, because `mods/<id>/` is the only place a mod is ever mounted. */
  for (size_t i = 0; i < sizeof(kRootKeys) / sizeof(*kRootKeys); ++i) {
    if (!RootSlot(out, i)->empty()) continue;
    for (const std::string &id : out.Depends) {
      FBMod dep;
      if (!Manifest(dir + "/../" + id, root + "/../" + id, dep, nullptr)) continue;
      if (RootSlot(dep, i)->empty()) continue;
      *RootSlot(out, i) = *RootSlot(dep, i);
      break;
    }
    if (RootSlot(out, i)->empty()) {
      if (err) *err = std::string("mod.json has no \"") + kRootKeys[i] + "\" and no dependency supplies one";
      return false;
    }
  }
  if (!out.Dem.empty()) out.Dem = out.Data + "/" + out.Dem;
  return true;
}

} // namespace FlightBox::Missions
