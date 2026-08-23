#ifndef OUTSHINE_SCENARIO_MOD_H
#define OUTSHINE_SCENARIO_MOD_H

#include <string>
#include <string_view>
#include <vector>

#include "Scene.h"

namespace outshine::SceneLegacy {

class Mod {
public:

  [[nodiscard]] bool Read(const std::string &text, const std::string &path);
  [[nodiscard]] bool Load(const std::string &root, const std::string &name);

  const std::string &Name() const { return Name_; }
  const std::string &Path() const { return Path_; }
  const std::string &Error() const { return Error_; }

  const Scene *Find(const std::string &id) const;
  const std::vector<Scene> &Scenes() const { return Scenes_; }
  std::string Ids() const;

private:
  [[nodiscard]] bool Refuse(std::string_view why);

  std::string Name_, Path_, Error_;
  std::vector<Scene> Scenes_;
};

}
#endif
