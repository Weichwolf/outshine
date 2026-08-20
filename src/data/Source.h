#ifndef SOURCE_H
#define SOURCE_H

#include <cstdint>

#include "Fetched.h"
#include "Request.h"
#include "SourceDecl.h"
#include "Transport.h"

namespace outshine::Data {

enum class Coverage : uint8_t { Inside, Outside };

class Source {
public:
  virtual ~Source() = default;

  [[nodiscard]] virtual const SourceDecl &Declaration() const noexcept = 0;

  [[nodiscard]] virtual Coverage Covers(const Request &request) const noexcept = 0;

  [[nodiscard]] virtual Address Serves(const Request &request) const noexcept = 0;

  [[nodiscard]] virtual Ticket Begin(const Address &at, Transport &transport) const = 0;

  [[nodiscard]] virtual Fetched Collect(const Address &at, Ticket ticket,
                                        Transport &transport) const = 0;
};

}
#endif
