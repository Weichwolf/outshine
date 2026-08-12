#ifndef SERVERARTIFACTS_H
#define SERVERARTIFACTS_H

#include <memory>
#include <vector>

#include "Artifacts.h"
#include "HttpPost.h"

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
  [[nodiscard]] bool MakeDir(const std::string &) override { return true; }
  [[nodiscard]] Delivery Png(const std::string &name, const uint8_t *rgba, int width, int height) override;
  [[nodiscard]] Delivery Bytes(const std::string &name, const void *data, size_t bytes) override;
  [[nodiscard]] Delivery Text(const std::string &name, const std::string &text) override;
  [[nodiscard]] Delivery Settle() override;

private:
  std::string Url(const std::string &name) const;
  /* Forgets every post whose verdict has arrived, so Flight_ holds what is on the wire and nothing
   * else. */
  void Reap();
  [[nodiscard]] Delivery Send(const std::string &name, const void *data, size_t bytes, const char *contentType);

  std::string Base_, RunId_;
  /* The posts still on the wire. A finished one is forgotten as soon as its verdict has been read,
   * so this holds the products in flight and nothing else. */
  std::vector<std::unique_ptr<HttpPost>> Flight_;
  bool Refused_ = false;
};

} // namespace outshine::Clients
#endif
