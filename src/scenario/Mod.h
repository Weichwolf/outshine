#ifndef MOD_H
#define MOD_H

#include <string>
#include <vector>

#include "Scene.h"

namespace outshine::Scenario {

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
  [[nodiscard]] bool Refuse(const std::string &why);

  std::string Name_, Path_, Error_;
  std::vector<Scene> Scenes_;
};

}
#endif
