#ifndef OUTSHINE_BASE_MATH_DECIMALEDGE_H
#define OUTSHINE_BASE_MATH_DECIMALEDGE_H

#include <cmath>
#include <charconv>
#include <optional>
#include <string_view>

namespace outshine {

constexpr unsigned kExponentReach = 62U;
constexpr long long kExponentSpan = static_cast<long long>(1ULL << kExponentReach);

enum class Edge { Zero, Infinity };

[[nodiscard]] inline std::optional<long long> LeadingPlaceOf(std::string_view whole,
                                                             std::string_view fraction) {
  for (size_t at = 0; at < whole.size(); ++at) {
    if (whole[at] != '0') { return static_cast<long long>(whole.size() - 1 - at); }
  }
  for (size_t at = 0; at < fraction.size(); ++at) {
    if (fraction[at] != '0') { return -static_cast<long long>(at + 1); }
  }
  return std::nullopt;
}

[[nodiscard]] inline long long ExponentOf(std::string_view number, size_t e) {
  if (e == std::string_view::npos || e + 1 >= number.size()) { return 0; }
  const char *from = number.data() + e + 1;
  const char *const stop = number.data() + number.size();
  const bool shrinks = *from == '-';
  if (*from == '+' || *from == '-') { ++from; }
  long long shift = 0;
  if (std::from_chars(from, stop, shift).ec != std::errc()) {
    return shrinks ? -kExponentSpan - 1 : kExponentSpan + 1;
  }
  return shrinks ? -shift : shift;
}

[[nodiscard]] inline Edge DecimalEdge(std::string_view number) {
  if (!number.empty() && (number.front() == '+' || number.front() == '-')) {
    number.remove_prefix(1);
  }
  const size_t e = number.find_first_of("eE");
  const std::string_view digits = number.substr(0, e);
  const size_t dot = digits.find('.');
  const std::string_view whole =
      digits.substr(0, dot == std::string_view::npos ? digits.size() : dot);
  const std::string_view fraction =
      dot == std::string_view::npos ? std::string_view() : digits.substr(dot + 1);

  const std::optional<long long> lead = LeadingPlaceOf(whole, fraction);
  if (!lead) { return Edge::Zero; }
  const long long shift = ExponentOf(number, e);
  if (shift > kExponentSpan) { return Edge::Infinity; }
  if (shift < -kExponentSpan) { return Edge::Zero; }
  return *lead + shift < 0 ? Edge::Zero : Edge::Infinity;
}

} // namespace outshine
#endif
