#ifndef FILEARTIFACTS_H
#define FILEARTIFACTS_H

#include "Artifacts.h"

namespace outshine::Clients {

/* A run's products under one root directory. The root is the ONE thing about a run that the
 * declaration cannot know, because it is where this machine keeps its scratch — see
 * doc/build-and-ops.md for the three environment values and why each one is environment. */
class FileArtifacts : public Artifacts {
public:
  explicit FileArtifacts(const std::string &root) : Root_(root) {}

  bool MakeDir(const std::string &name) override;
  bool Png(const std::string &name, const uint8_t *rgba, int width, int height) override;
  bool Bytes(const std::string &name, const void *data, size_t bytes) override;
  bool Text(const std::string &name, const std::string &text) override;

private:
  std::string Resolve(const std::string &name) const;

  std::string Root_;
};

} // namespace outshine::Clients
#endif
