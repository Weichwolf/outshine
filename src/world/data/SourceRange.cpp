#include "SourceRange.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "Sha256.h"

namespace outshine::Data {
namespace {
constexpr std::string_view kPrefix = "range/1\n";
constexpr size_t kDigestBytes = 64;

std::optional<std::string_view> Line(std::string_view &remaining) {
  const size_t newline = remaining.find('\n');
  if (newline > MaximumEntityTagBytes) { return std::nullopt; }
  const auto line = remaining.substr(0, newline);
  remaining.remove_prefix(newline + 1);
  return line;
}

std::optional<uint64_t> Number(std::string_view &remaining) {
  const auto line = Line(remaining);
  if (!line || line->empty()) { return std::nullopt; }
  uint64_t value = 0;
  const auto parsed = std::from_chars(line->data(), line->data() + line->size(), value);
  return parsed.ec == std::errc() && parsed.ptr == line->data() + line->size()
             ? std::optional(value)
             : std::nullopt;
}
}

std::optional<std::vector<uint8_t>> PackSourceRange(std::span<const uint8_t> bytes,
                                                    const RangeResponse &origin) {
  if (!origin.Valid(bytes.size()) ||
      bytes.size() > std::numeric_limits<size_t>::max() - MaximumRangeRecordOverhead) {
    return std::nullopt;
  }
  std::string header(kPrefix);
  header += std::to_string(origin.Bytes.First) + '\n';
  header += std::to_string(origin.Bytes.Length) + '\n';
  header += std::to_string(origin.TotalBytes) + '\n';
  header += origin.EntityTag + '\n';
  std::vector<uint8_t> record;
  record.reserve(header.size() + bytes.size() + kDigestBytes);
  record.insert(record.end(), header.begin(), header.end());
  record.insert(record.end(), bytes.begin(), bytes.end());
  const auto digest = Sha256Hex(record.data(), record.size());
  record.insert(record.end(), digest.begin(), digest.end());
  return record;
}

std::optional<RangeResponse> UnpackSourceRange(std::vector<uint8_t> &record) {
  if (record.size() <= kPrefix.size() + kDigestBytes) { return std::nullopt; }
  const std::string_view sealed(reinterpret_cast<const char *>(record.data()), record.size());
  const auto encoded = sealed.substr(0, sealed.size() - kDigestBytes);
  if (!encoded.starts_with(kPrefix) || Sha256Hex(encoded) != sealed.substr(encoded.size())) {
    return std::nullopt;
  }
  auto remaining = encoded.substr(kPrefix.size());
  const auto first = Number(remaining);
  const auto length = Number(remaining);
  const auto total = Number(remaining);
  const auto tag = Line(remaining);
  if (!first || !length || !total || !tag) { return std::nullopt; }
  RangeResponse origin{.Bytes = {.First = *first, .Length = *length},
                       .TotalBytes = *total,
                       .EntityTag = std::string(*tag)};
  if (!origin.Valid(remaining.size()) ||
      sealed.size() - remaining.size() > MaximumRangeRecordOverhead) {
    return std::nullopt;
  }
  const auto headerBytes = encoded.size() - remaining.size();
  record.resize(encoded.size());
  record.erase(record.begin(), record.begin() + static_cast<ptrdiff_t>(headerBytes));
  return origin;
}

}
