#ifndef OUTSHINE_WORLD_DATA_FETCH_H
#define OUTSHINE_WORLD_DATA_FETCH_H

#include <string>
#include <optional>
#include <string_view>
#include <utility>

#include "Address.h"
#include "DataKind.h"
#include "Transport.h"

namespace outshine::Data {

/// Owned demand borrowing no source or storage. Construction does not perform IO.
class Fetch {
public:
  /// Copy a category and address without allocation.
  /// @param kind Native data category.
  /// @param where Requested source address.
  Fetch(DataKind kind, Address where) : Kind_(kind), Where_(where) {}

  /// Request one original byte interval, optionally pinned to a strong HTTP ETag.
  /// @param kind Native data category.
  /// @param where Source-object address, independent of render ownership.
  /// @param range Original byte interval; acquisition validates bounds and capacity.
  /// @param entityTag Owned If-Match value, including quotes; empty allows bootstrap.
  Fetch(DataKind kind, Address where, ByteRange range, std::string entityTag = {})
      : Kind_(kind), Where_(where), Range_(range), EntityTag_(std::move(entityTag)) {}

  /// Read the category in constant time.
  /// @return Stored category; no allocation.
  [[nodiscard]] DataKind Kind() const noexcept { return Kind_; }

  /// Borrow the owned address in constant time.
  /// @return Reference valid until this request is destroyed or replaced.
  [[nodiscard]] const Address &Where() const noexcept { return Where_; }

  /// Inspect partial demand; empty requests the whole declared payload.
  /// @return Borrowed interval; valid until this request is replaced or destroyed.
  [[nodiscard]] const std::optional<ByteRange> &Range() const noexcept { return Range_; }

  /// Borrow the optional source revision pin without allocation.
  /// @return If-Match value; valid until this request is replaced or destroyed.
  [[nodiscard]] std::string_view EntityTag() const noexcept { return EntityTag_; }

  /// Serialize category and address for diagnostics.
  /// @return Owned string; may allocate.
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
