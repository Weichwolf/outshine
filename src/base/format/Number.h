#ifndef OUTSHINE_BASE_FORMAT_NUMBER_H
#define OUTSHINE_BASE_FORMAT_NUMBER_H

#include <charconv>
#include <cmath>
#include <expected>
#include <string_view>
#include <system_error>

namespace outshine {

enum class NumberError { InvalidSyntax, OutOfRange, NonFinite };

[[nodiscard]] inline std::expected<double, NumberError>
ParseFiniteNumber(std::string_view text) noexcept {
  if (text.starts_with('+')) {
    text.remove_prefix(1);
    if (text.starts_with('-')) { return std::unexpected(NumberError::InvalidSyntax); }
  }
  if (text.empty()) { return std::unexpected(NumberError::InvalidSyntax); }
  double value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ptr != text.data() + text.size() || parsed.ec == std::errc::invalid_argument) {
    return std::unexpected(NumberError::InvalidSyntax);
  }
  if (parsed.ec == std::errc::result_out_of_range) {
    return std::unexpected(NumberError::OutOfRange);
  }
  if (!std::isfinite(value)) { return std::unexpected(NumberError::NonFinite); }
  return value;
}

}

#endif
