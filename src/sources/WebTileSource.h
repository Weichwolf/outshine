#ifndef OUTSHINE_SOURCES_WEBTILESOURCE_H
#define OUTSHINE_SOURCES_WEBTILESOURCE_H

#include <cstddef>
#include <string>
#include <utility>

#include <Source.h>

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

  [[nodiscard]] Coverage Covers(const SourceRequest &request) const noexcept final;
  [[nodiscard]] Address Serves(const SourceRequest &request) const noexcept final;
  [[nodiscard]] FetchStart Begin(const Address &at, SourceTransport &transport) const final;
  [[nodiscard]] SourceResponse
  Collect(const Address &at, Ticket ticket, SourceTransport &transport) const final;

protected:
  WebTileSource(SourceDecl decl, std::string endpoint)
      : Decl_(std::move(decl)), Endpoint_(std::move(endpoint)) {}

  [[nodiscard]] std::string Url(const Address &at) const;

  [[nodiscard]] Meaning Classify(Replied said) const noexcept;

  [[nodiscard]] virtual bool
  AcceptsIdentity([[maybe_unused]] const Wire::Response &response) const noexcept {
    return true;
  }

  [[nodiscard]] virtual bool AcceptsEmptyPayload() const noexcept { return false; }

  [[nodiscard]] virtual bool CountsAbsent(int status) const noexcept {
    return status == kHttpNotFound;
  }

private:
  SourceDecl Decl_;
  std::string Endpoint_;
};

}
#endif
