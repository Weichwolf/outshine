#include "SourceSession.h"
#include <expected>
#include <memory>
#include <string>
#include <utility>

namespace outshine::Data {
SourceSession::Read::Read(std::shared_ptr<SourceSession> session,
                          const SourceRequest &request,
                          SourceTransport &transport)
    : Session_(std::move(session)),
      Transport_(&transport),
      Query_(Session_->Sources().Ask(request)) {}

SourceSession::Read::Read(Read &&other) noexcept
    : Session_(std::move(other.Session_)),
      Transport_(std::exchange(other.Transport_, nullptr)),
      Query_(std::move(other.Query_)) {
  other.Query_.reset();
}

SourceSession::Read &SourceSession::Read::operator=(Read &&other) {
  if (this == &other) { return *this; }
  Cancel();
  Session_ = std::move(other.Session_);
  Transport_ = std::exchange(other.Transport_, nullptr);
  if (other.Query_) {
    Query_.emplace(std::move(*other.Query_));
    other.Query_.reset();
  }
  return *this;
}

SourceSession::Read::~Read() {
  Cancel();
}

SourceDelivery SourceSession::Read::Poll() {
  if (!Session_ || !Query_ || Transport_ == nullptr) { return SourceDelivery::Consumed(); }
  return Session_->Sources().Collect(*Query_, *Transport_);
}

void SourceSession::Read::Cancel() {
  if (Session_ && Query_ && Transport_ != nullptr) { SourceSet::Abandon(*Query_, *Transport_); }
  Query_.reset();
  Session_.reset();
  Transport_ = nullptr;
}

std::expected<SourceSession::Read, std::string>
SourceSession::Request(std::shared_ptr<SourceSession> session,
                       const SourceRequest &request,
                       SourceTransport &transport) {
  if (!session) { return std::unexpected("source read requires a configured session"); }
  return Read(std::move(session), request, transport);
}
}
