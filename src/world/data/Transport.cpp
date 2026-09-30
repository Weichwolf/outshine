#include <world/data/Transport.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace outshine::Data {

namespace {
constexpr unsigned char kFirstVisibleAscii = 0x21;
constexpr unsigned char kDeleteAscii = 0x7f;
}

bool StrongEntityTag(std::string_view tag) noexcept {
  return tag.size() >= 2 && tag.size() <= MaximumEntityTagBytes && tag.front() == '"' &&
         tag.back() == '"' &&
         std::ranges::all_of(tag.substr(1, tag.size() - 2), [](unsigned char c) {
           return c >= kFirstVisibleAscii && c != '"' && c != kDeleteAscii;
         });
}

bool ByteRange::Valid() const noexcept {
  return Length > 0 && First <= std::numeric_limits<uint64_t>::max() - (Length - 1);
}

bool RangeResponse::Valid(size_t received) const noexcept {
  return Bytes.Valid() && Bytes.Length == received && Bytes.First < TotalBytes &&
         Bytes.Length <= TotalBytes - Bytes.First && StrongEntityTag(EntityTag);
}

}
