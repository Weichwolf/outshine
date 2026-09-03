#ifndef OUTSHINE_WORLD_DATA_FETCH_H
#define OUTSHINE_WORLD_DATA_FETCH_H

#include <string>

#include "Address.h"
#include "DataKind.h"

namespace outshine::Data {

class Fetch {
public:
  Fetch(DataKind kind, Address where) : Kind_(kind), Where_(where) {}

  [[nodiscard]] DataKind Kind() const noexcept { return Kind_; }

  [[nodiscard]] const Address &Where() const noexcept { return Where_; }

  [[nodiscard]] std::string Key() const { return std::string(Name(Kind_)) + "/" + Where_.Text(); }

private:
  DataKind Kind_;
  Address Where_;
};

} // namespace outshine::Data
#endif
