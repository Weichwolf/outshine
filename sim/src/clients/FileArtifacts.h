#ifndef FILEARTIFACTS_H
#define FILEARTIFACTS_H

#include "Artifacts.h"

namespace outshine::Clients {

/* A run's products under one root directory. The root is the ONE thing about a run that the
 * declaration cannot know, because it is where this machine keeps its scratch. */
class FileArtifacts : public Artifacts {
public:
  explicit FileArtifacts(const std::string &root) : Root_(root) {}

  bool MakeDir(const std::string &name) override;
  Delivery Png(const std::string &name, const uint8_t *rgba, int width, int height) override;
  Delivery Bytes(const std::string &name, const void *data, size_t bytes) override;
  Delivery Text(const std::string &name, const std::string &text) override;
  /* A write to a file system either happened on the line that asked for it or it did not. */
  Delivery Settle() override { return Delivery::Complete; }

private:
  std::string Resolve(const std::string &name) const;

  std::string Root_;
};

} // namespace outshine::Clients
#endif
