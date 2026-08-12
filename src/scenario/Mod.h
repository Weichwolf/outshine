#ifndef MOD_H
#define MOD_H

#include <string>
#include <vector>

#include "Scene.h"

namespace outshine::Scenario {

/* A MOD IS A SET OF DECLARED SCENES and nothing else — `<root>/<name>/mod.json`. It brings no code
 * and no world; what it brings is every stage, every clock and every recording this engine will ever
 * be asked for, in the one language that is schema-checkable, diffable and generable. A consumer
 * chooses a mod and a scene, and that is the whole command line.
 *
 * THERE IS NO PARTIAL MOD AND NO FALLBACK MOD. One refused scene refuses the file: a set that loaded
 * nine scenes out of ten would hand its consumer a world that is silently not the declared one. */
class Mod {
public:
  /* The bytes, so a caller that already has them — a test, or a source that is not a file — does not
   * have to put them on a disk first. `path` only names the declaration in a refusal. */
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

} // namespace outshine::Scenario
#endif
