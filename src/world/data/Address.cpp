#include "Address.h"

#include <array>
#include <charconv>
#include <system_error>
#include <string>
#include <cstddef>

namespace outshine::Data {

constexpr size_t kTextBytes = 48;

namespace {

char *Wrote(char *at, char *end, long long value) {
  const std::to_chars_result put = std::to_chars(at, end, value);
  return put.ec == std::errc() ? put.ptr : at;
}

} // namespace

std::string Address::Text() const {
  std::array<char, kTextBytes> text{};
  char *at = text.data();
  char *const end = text.data() + text.size();
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
  return {text.data(), static_cast<size_t>(at - text.data())};
}

} // namespace outshine::Data
