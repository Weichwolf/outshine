#ifndef SERVERARTIFACTS_H
#define SERVERARTIFACTS_H

#include "Artifacts.h"

namespace outshine::Clients {

/* A RUN'S PRODUCTS FROM THE BROWSER. There is no filesystem to write to and no reason to invent one:
 * the products go where the log goes, so a browser run and a native run leave the same evidence in
 * the same place. `POST /artifact/<runId>-<name>` — the collector's name alphabet has no separator,
 * so the declared path's slashes become dashes and the run id prefixes it. */
class ServerArtifacts : public Artifacts {
public:
  ServerArtifacts(const std::string &base, const std::string &runId)
      : Base_(base), RunId_(runId) {}

  /* There are no directories on the far end; a declared directory is a name prefix. */
  bool MakeDir(const std::string &) override { return true; }
  bool Png(const std::string &name, const uint8_t *rgba, int width, int height) override;
  bool Bytes(const std::string &name, const void *data, size_t bytes) override;
  bool Text(const std::string &name, const std::string &text) override;

private:
  std::string Url(const std::string &name) const;

  std::string Base_, RunId_;
};

} // namespace outshine::Clients
#endif
