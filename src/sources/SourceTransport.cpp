#include <SourceTransport.h>

#include <algorithm>
#include <cstddef>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
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

std::optional<std::string_view> SingleResponseHeader(std::string_view headers,
                                                     std::string_view name) noexcept {
  if (headers.size() > MaximumResponseHeaderBytes || name.empty()) { return std::nullopt; }
  std::optional<std::string_view> found;
  while (!headers.empty()) {
    const auto end = headers.find('\n');
    auto line = headers.substr(0, end);
    headers = end == std::string_view::npos ? std::string_view{} : headers.substr(end + 1);
    if (!line.empty() && (line.front() == ' ' || line.front() == '\t')) { return std::nullopt; }
    const auto colon = line.find(':');
    if (colon == std::string_view::npos) { continue; }
    const auto field = line.substr(0, colon);
    if (!std::ranges::equal(field, name, [](unsigned char left, unsigned char right) {
          return std::tolower(left) == std::tolower(right);
        })) {
      continue;
    }
    if (found) { return std::nullopt; }
    line.remove_prefix(colon + 1);
    const auto first = line.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
      found = std::string_view{};
    } else {
      const auto last = line.find_last_not_of(" \t\r");
      found = line.substr(first, last - first + 1);
    }
  }
  return found;
}

bool ByteRange::Valid() const noexcept {
  return Length > 0 && First <= std::numeric_limits<uint64_t>::max() - (Length - 1);
}

bool RangeResponse::Valid(size_t received) const noexcept {
  return Bytes.Valid() && Bytes.Length == received && Bytes.First < TotalBytes &&
         Bytes.Length <= TotalBytes - Bytes.First && StrongEntityTag(EntityTag);
}

}
