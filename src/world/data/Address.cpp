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

void Mark(char *&at, const char *end, char value) {
  if (at < end) { *at++ = value; }
}

}

std::string Address::Text() const {
  std::array<char, kTextBytes> text{};
  char *at = text.data();
  char *const end = text.data() + text.size();
  if (const auto tile = Tile()) {
    at = Wrote(at, end, tile->Zoom);
    Mark(at, end, '/');
    at = Wrote(at, end, tile->X);
    Mark(at, end, '/');
    at = Wrote(at, end, tile->Y);
  } else if (const auto geo = GeoCell()) {
    Mark(at, end, 'q');
    Mark(at, end, '/');
    at = Wrote(at, end, geo->Level);
    Mark(at, end, '/');
    at = Wrote(at, end, geo->X);
    Mark(at, end, '/');
    at = Wrote(at, end, geo->Y);
  } else if (const auto cell = Cell()) {
    Mark(at, end, 'g');
    Mark(at, end, '/');
    at = Wrote(at, end, cell->SouthDeg);
    Mark(at, end, '/');
    at = Wrote(at, end, cell->WestDeg);
  } else if (const auto index = Index()) {
    Mark(at, end, 'w');
    Mark(at, end, '/');
    at = Wrote(at, end, *index);
  }
  return {text.data(), static_cast<size_t>(at - text.data())};
}

}
