#ifndef ARTIFACTS_H
#define ARTIFACTS_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace outshine::Clients {

/* WHERE A RUN'S PRODUCTS GO. The runs themselves are declared and identical in both translations;
 * only the destination differs — a directory natively, an HTTP endpoint in the browser — so that is
 * the one thing behind an interface. Names are declared paths relative to the sink's own root; the
 * sink resolves them and refuses anything it will not store. */
class Artifacts {
public:
  virtual ~Artifacts() = default;

  virtual bool MakeDir(const std::string &name) = 0;
  virtual bool Png(const std::string &name, const uint8_t *rgba, int width, int height) = 0;
  virtual bool Bytes(const std::string &name, const void *data, size_t bytes) = 0;
  virtual bool Text(const std::string &name, const std::string &text) = 0;
};

} // namespace outshine::Clients
#endif
