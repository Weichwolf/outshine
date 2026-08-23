#include "Mod.h"

#include <fstream>
#include <sstream>

#include "Fields.h"

namespace outshine::SceneLegacy {

bool Mod::Refuse(std::string_view why) {
  Error_ = Path_ + ": " + std::string(why);
  Scenes_.clear();
  return false;
}

bool Mod::Read(const std::string &text, const std::string &path) {
  Path_ = path;
  Scenes_.clear();

  Json doc;
  if (!doc.Parse(text.c_str(), text.size()))

    return Refuse("is not valid JSON, stopping at byte " + std::to_string(doc.StoppedAt()));

  std::string err;
  Fields root(doc.Root(), "mod", err);
  std::string schema;
  if (!root.NeedString("schema", schema)) return Refuse(err);
  if (schema != "outshine/mod/1") return Refuse("schema is not outshine/mod/1: " + schema);
  if (!root.NeedString("name", Name_)) return Refuse(err);
  std::string subject;
  if (!root.NeedString("subject", subject)) return Refuse(err);

  const Json::Ref scenes = root.Child("scenes");
  if (scenes.GetKind() != Json::Kind::Array || scenes.Size() == 0)
    return Refuse("needs a non-empty scenes array");
  if (!root.Closed()) return Refuse(err);

  Scenes_.resize(scenes.Size());
  for (size_t i = 0; i < scenes.Size(); i++)
    if (!Scenes_[i].Read(scenes[i], "scenes[" + std::to_string(i) + "]", err)) return Refuse(err);

  for (size_t i = 0; i < Scenes_.size(); i++)
    for (size_t j = i + 1; j < Scenes_.size(); j++)
      if (Scenes_[i].Id() == Scenes_[j].Id())
        return Refuse("duplicate scene id " + Scenes_[i].Id());
  return true;
}

bool Mod::Load(const std::string &root, const std::string &name) {
  Name_ = name;
  Path_ = root + "/" + name + "/mod.json";
  std::ifstream f(Path_, std::ios::binary);
  if (!f) {
    Error_ = "cannot open " + Path_;
    return false;
  }
  std::ostringstream buf;
  buf << f.rdbuf();
  return Read(buf.str(), Path_);
}

const Scene *Mod::Find(const std::string &id) const {
  for (const Scene &s : Scenes_)
    if (s.Id() == id) return &s;
  return nullptr;
}

std::string Mod::Ids() const {
  std::string out;
  for (const Scene &s : Scenes_) out += (out.empty() ? "" : " ") + s.Id();
  return out;
}

}
