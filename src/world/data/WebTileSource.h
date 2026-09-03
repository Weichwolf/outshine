#ifndef OUTSHINE_WORLD_DATA_WEBTILESOURCE_H
#define OUTSHINE_WORLD_DATA_WEBTILESOURCE_H

#include <cstddef>
#include <string>
#include <utility>

#include "Source.h"

namespace outshine::Data {

constexpr int kHttpOk = 200;
constexpr int kHttpForbidden = 403;
constexpr int kHttpNotFound = 404;
constexpr int kHttpTimeout = 408;
constexpr int kHttpTooMany = 429;
constexpr int kHttpServerFirst = 500;

struct Replied {
  int Status = 0;
  size_t Bytes = 0;
};

class WebTileSource : public Source {
public:
  [[nodiscard]] const SourceDecl &Declaration() const noexcept final { return Decl_; }

  [[nodiscard]] Coverage Covers(const Fetch &request) const noexcept final;
  [[nodiscard]] Address Serves(const Fetch &request) const noexcept final;
  [[nodiscard]] Ticket Begin(const Address &at, Transport &transport) const final;
  [[nodiscard]] Fetched Collect(const Address &at, Ticket ticket, Transport &transport) const final;

protected:
  explicit WebTileSource(SourceDecl decl) : Decl_(std::move(decl)) {}

  [[nodiscard]] virtual std::string Url(const Address &at) const = 0;

  [[nodiscard]] Meaning Classify(Replied said) const noexcept;

  [[nodiscard]] virtual bool CountsAbsent(int status) const noexcept {
    return status == kHttpNotFound;
  }

private:
  SourceDecl Decl_;
};

} // namespace outshine::Data
#endif
