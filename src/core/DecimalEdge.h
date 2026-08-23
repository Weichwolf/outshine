#ifndef OUTSHINE_CORE_DECIMALEDGE_H
#define OUTSHINE_CORE_DECIMALEDGE_H

#include <cmath>
#include <charconv>
#include <string_view>

namespace outshine {

enum class Edge { Zero, Infinity };

// the ONE judge for a decimal that from_chars reported out of range: the first
// significant digit's effective decimal exponent decides underflow from overflow --
// fraction-spelled tininess is zero, mantissa-borne hugeness is infinity, whatever the
// written exponent's sign says (board:1701, its recurrence 1740)
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
  long long lead = 0;
  bool found = false;
  for (size_t at = 0; at < whole.size() && !found; ++at) {
    if (whole[at] != '0') {
      lead = (long long)(whole.size() - 1 - at);
      found = true;
    }
  }
  for (size_t at = 0; at < fraction.size() && !found; ++at) {
    if (fraction[at] != '0') {
      lead = -(long long)(at + 1);
      found = true;
    }
  }
  if (!found) { return Edge::Zero; }
  long long shift = 0;
  if (e != std::string_view::npos && e + 1 < number.size()) {
    const char *from = number.data() + e + 1;
    const char *const stop = number.data() + number.size();
    const bool shrinks = *from == '-';
    if (*from == '+' || *from == '-') { ++from; }
    if (std::from_chars(from, stop, shift).ec != std::errc()) {
      // an exponent past long long dwarfs any spelled significand -- its sign decides
      return shrinks ? Edge::Zero : Edge::Infinity;
    }
    if (shrinks) { shift = -shift; }
    // a near-LLONG_MAX exponent would overflow the sum below -- it already decides alone
    if (shift > (1LL << 62)) { return Edge::Infinity; }
    if (shift < -(1LL << 62)) { return Edge::Zero; }
  }
  return lead + shift < 0 ? Edge::Zero : Edge::Infinity;
}

}
#endif
