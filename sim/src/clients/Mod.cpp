#include "Mod.h"

#include <fstream>
#include <sstream>

namespace outshine::Clients {

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
  const std::string text = buf.str();

  Json doc;
  if (!doc.Parse(text.c_str(), text.size())) {
    Error_ = Path_ + ": not valid JSON";
    return false;
  }
  const Json::Ref root_ = doc.Root();
  const Json::Ref scenes = root_["scenes"];
  if (scenes.GetKind() != Json::Kind::Array || scenes.Size() == 0) {
    Error_ = Path_ + ": needs a non-empty scenes array";
    return false;
  }
  Scenes_.resize(scenes.Size());
  for (size_t i = 0; i < scenes.Size(); i++) {
    if (Scenes_[i].Read(scenes[i], Error_)) continue;
    Error_ = Path_ + ": " + Error_;
    Scenes_.clear();
    return false;
  }
  for (size_t i = 0; i < Scenes_.size(); i++)
    for (size_t j = i + 1; j < Scenes_.size(); j++)
      if (Scenes_[i].Id() == Scenes_[j].Id()) {
        Error_ = Path_ + ": duplicate scene id " + Scenes_[i].Id();
        Scenes_.clear();
        return false;
      }
  return true;
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

}  // namespace outshine::Clients
