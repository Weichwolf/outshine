#ifndef MOD_H
#define MOD_H

#include <string>
#include <vector>

#include "Scene.h"

namespace outshine::Clients {

/* A MOD IS A SET OF DECLARED SCENES and nothing else — `<root>/<name>/mod.json`. It brings no code
 * and no world; what it brings is every standpoint, every clock and every recording
 * this engine will ever be asked for, in the one language that is schema-checkable, diffable and
 * generable. The entry point chooses a mod and a scene, and that is the whole command line. */
class Mod {
public:
  bool Load(const std::string &root, const std::string &name);

  const std::string &Name() const { return Name_; }
  const std::string &Path() const { return Path_; }
  const std::string &Error() const { return Error_; }

  const Scene *Find(const std::string &id) const;
  const std::vector<Scene> &Scenes() const { return Scenes_; }
  std::string Ids() const;

private:
  std::string Name_, Path_, Error_;
  std::vector<Scene> Scenes_;
};

} // namespace outshine::Clients
#endif
