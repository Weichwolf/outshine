#include <world/data/Address.h>

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

}

std::string Address::Text() const {
  std::array<char, kTextBytes> text{};
  char *at = text.data();
  char *const end = text.data() + text.size();
  if (const auto tile = Tile()) {
    at = Wrote(at, end, tile->Zoom);
    if (at < end) { *at++ = '/'; }
    at = Wrote(at, end, tile->X);
    if (at < end) { *at++ = '/'; }
    at = Wrote(at, end, tile->Y);
  } else if (const auto cell = Cell()) {
    if (at < end) { *at++ = 'g'; }
    if (at < end) { *at++ = '/'; }
    at = Wrote(at, end, cell->SouthDeg);
    if (at < end) { *at++ = '/'; }
    at = Wrote(at, end, cell->WestDeg);
  } else {
    if (at < end) { *at++ = 'w'; }
    if (at < end) { *at++ = '/'; }
    at = Wrote(at, end, *Index());
  }
  return {text.data(), static_cast<size_t>(at - text.data())};
}

}
