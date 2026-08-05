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

bool Dir(const std::string &text, const std::string &root, const char *key, std::string &out,
         std::string *err) {
  std::string rel;
  if (!Field(text, key, rel)) {
    if (err) *err = std::string("mod.json has no \"") + key + "\"";
    return false;
  }
  out = root + "/" + rel;
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

bool FBLoadMod(const std::string &dir, FBMod &out, std::string *err) {
  const std::string path = dir + "/mod.json";
  std::ifstream in(path);
  if (!in) { if (err) *err = "cannot open " + path; return false; }
  std::stringstream buf; buf << in.rdbuf();
  const std::string text = buf.str();

  out.Dir = dir;
  Field(text, "id", out.Id);
  Field(text, "name", out.Name);
  return Dir(text, dir, "aircraft", out.Aircraft, err) &&
         Dir(text, dir, "models", out.Models, err) &&
         Dir(text, dir, "missions", out.Missions, err) &&
         Dir(text, dir, "campaigns", out.Campaigns, err) &&
         Dir(text, dir, "data", out.Data, err);
}

} // namespace FlightBox::Missions
