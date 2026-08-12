#ifndef REQUEST_H
#define REQUEST_H

#include <string>

#include "Address.h"
#include "DataKind.h"

namespace outshine::Data {

/* ONE QUESTION PUT TO THE REGISTRY. It names the kind and the place and nothing about who answers:
 * which source is asked is the selector's decision and never the caller's, which is what makes a
 * second upstream a registration rather than an edit at the call site. */
class Request {
public:
  Request(DataKind kind, Address where) : Kind_(kind), Where_(where) {}

  [[nodiscard]] DataKind Kind() const noexcept { return Kind_; }
  [[nodiscard]] const Address &Where() const noexcept { return Where_; }

  /* What the byte cache keys on. Source-free on purpose: the same question asked twice must find one
   * entry however the registry resolved it the first time. */
  [[nodiscard]] std::string Key() const { return std::string(Name(Kind_)) + "/" + Where_.Text(); }

private:
  DataKind Kind_;
  Address Where_;
};

} // namespace outshine::Data
#endif
