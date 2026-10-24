#ifndef OUTSHINE_SOURCES_SOURCEREQUEST_H
#define OUTSHINE_SOURCES_SOURCEREQUEST_H

#include <string>
#include <optional>
#include <string_view>
#include <utility>

#include "Address.h"
#include "DataKind.h"
#include "Transport.h"

namespace outshine::Data {

class SourceRequest {
public:
  SourceRequest(DataKind kind, Address where) : Kind_(kind), Where_(where) {}

  SourceRequest(DataKind kind, Address where, ByteRange range, std::string entityTag = {})
      : Kind_(kind), Where_(where), Range_(range), EntityTag_(std::move(entityTag)) {}

  [[nodiscard]] DataKind Kind() const noexcept { return Kind_; }

  [[nodiscard]] const Address &Where() const noexcept { return Where_; }

  [[nodiscard]] const std::optional<ByteRange> &Range() const noexcept { return Range_; }

  [[nodiscard]] std::string_view EntityTag() const noexcept { return EntityTag_; }

  [[nodiscard]] std::string Key() const {
    std::string key = std::string(Name(Kind_)) + "/" + Where_.Text();
    if (Range_) {
      key += "/r/" + std::to_string(Range_->First) + "/" + std::to_string(Range_->Length);
      key += "/" + EntityTag_;
    }
    return key;
  }

private:
  DataKind Kind_;
  Address Where_;
  std::optional<ByteRange> Range_ = std::nullopt;
  std::string EntityTag_;
};

}
#endif
