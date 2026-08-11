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
  /* ACCEPTANCE IS NOT DELIVERY where the sink is a network: the three writers below say only that
   * the product was taken, and Settle() is where a run finds out whether it landed. A directory
   * answers Complete on the same line it writes. */
  enum class Delivery { InFlight, Complete, Refused };

  virtual ~Artifacts() = default;

  /* A directory is a precondition, not a product: both sinks can answer it on the line that asks. */
  virtual bool MakeDir(const std::string &name) = 0;
  virtual Delivery Png(const std::string &name, const uint8_t *rgba, int width, int height) = 0;
  virtual Delivery Bytes(const std::string &name, const void *data, size_t bytes) = 0;
  virtual Delivery Text(const std::string &name, const std::string &text) = 0;
  virtual Delivery Settle() = 0;
};

} // namespace outshine::Clients
#endif
