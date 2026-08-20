#ifndef WEBTILESOURCE_H
#define WEBTILESOURCE_H

#include <string>
#include <utility>

#include "Source.h"

namespace outshine::Data {

class WebTileSource : public Source {
public:
  [[nodiscard]] const SourceDecl &Declaration() const noexcept final { return Decl_; }
  [[nodiscard]] Coverage Covers(const Request &request) const noexcept final;
  [[nodiscard]] Address Serves(const Request &request) const noexcept final;
  [[nodiscard]] Ticket Begin(const Address &at, Transport &transport) const final;
  [[nodiscard]] Fetched Collect(const Address &at, Ticket ticket,
                                Transport &transport) const final;

protected:
  explicit WebTileSource(SourceDecl decl) : Decl_(std::move(decl)) {}

  [[nodiscard]] virtual std::string Url(const Address &at) const = 0;
  [[nodiscard]] virtual Meaning Classify(int status, size_t bytes) const noexcept = 0;

private:
  SourceDecl Decl_;
};

}
#endif
