#include "Address.h"

#include <charconv>

namespace outshine::Data {

namespace {

char *Wrote(char *at, char *end, long long value) {
  const std::to_chars_result put = std::to_chars(at, end, value);
  return put.ec == std::errc() ? put.ptr : at;
}

} // namespace

std::string Address::Text() const {
  char text[48];
  char *at = text;
  char *const end = text + sizeof text;
  if (How_ == Scheme::TileZxy) {
    at = Wrote(at, end, static_cast<long long>(Z_));
    if (at < end) { *at++ = '/'; }
    at = Wrote(at, end, static_cast<long long>(X_));
    if (at < end) { *at++ = '/'; }
    at = Wrote(at, end, static_cast<long long>(Y_));
  } else {
    if (at < end) { *at++ = 'w'; }
    if (at < end) { *at++ = '/'; }
    at = Wrote(at, end, static_cast<long long>(X_));
  }
  return std::string(text, static_cast<size_t>(at - text));
}

} // namespace outshine::Data
