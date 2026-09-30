#ifndef OUTSHINE_WORLD_DATA_FETCH_H
#define OUTSHINE_WORLD_DATA_FETCH_H

#include <string>

#include "Address.h"
#include "DataKind.h"

namespace outshine::Data {

/// Value-only demand borrowing no source or storage. Construction does not perform IO.
class Fetch {
public:
  /// Copy a category and address without allocation.
  /// @param kind Native data category.
  /// @param where Requested source address.
  Fetch(DataKind kind, Address where) : Kind_(kind), Where_(where) {}

  /// Read the category in constant time.
  /// @return Stored category; no allocation.
  [[nodiscard]] DataKind Kind() const noexcept { return Kind_; }

  /// Borrow the owned address in constant time.
  /// @return Reference valid until this request is destroyed or replaced.
  [[nodiscard]] const Address &Where() const noexcept { return Where_; }

  /// Serialize category and address for diagnostics.
  /// @return Owned string; may allocate.
  [[nodiscard]] std::string Key() const { return std::string(Name(Kind_)) + "/" + Where_.Text(); }

private:
  DataKind Kind_;
  Address Where_;
};

}
#endif
