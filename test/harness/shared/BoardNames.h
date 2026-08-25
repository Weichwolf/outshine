#pragma once

#include <cstddef>
#include <string_view>

namespace outshine::Test::Board {

inline constexpr size_t kDigits = 4;
inline constexpr size_t kMostNamed = 64;

inline constexpr std::string_view kMarkers[] = {"board:", "board/"};

struct Named {
  unsigned Items[kMostNamed] = {};
  size_t Count = 0;
  bool Overflowed = false;

  [[nodiscard]] constexpr bool Holds(unsigned item) const noexcept {
    for (size_t at = 0; at < Count; ++at) {
      if (Items[at] == item) { return true; }
    }
    return false;
  }
};

[[nodiscard]] constexpr bool DigitsAt(std::string_view text, size_t at) noexcept {
  if (at + kDigits > text.size()) { return false; }
  for (size_t step = 0; step < kDigits; ++step) {
    const char one = text[at + step];
    if (one < '0' || one > '9') { return false; }
  }
  return at + kDigits == text.size() || text[at + kDigits] < '0' || text[at + kDigits] > '9';
}

[[nodiscard]] constexpr unsigned NumberAt(std::string_view text, size_t at) noexcept {
  unsigned value = 0;
  for (size_t step = 0; step < kDigits; ++step) {
    value = value * 10 + unsigned(text[at + step] - '0');
  }
  return value;
}

[[nodiscard]] constexpr Named NamedIn(std::string_view text) noexcept {
  Named out;
  for (std::string_view marker : kMarkers) {
    for (size_t at = text.find(marker); at != std::string_view::npos;
         at = text.find(marker, at + 1)) {
      size_t from = at + marker.size();
      while (DigitsAt(text, from)) {
        if (out.Count == kMostNamed) {
          out.Overflowed = true;
          break;
        }
        out.Items[out.Count++] = NumberAt(text, from);
        from += kDigits;
        if (from < text.size() && text[from] == ',') {
          ++from;
          continue;
        }
        break;
      }
    }
  }
  return out;
}

static_assert(NamedIn("board:1844").Count == 1 && NamedIn("board:1844").Items[0] == 1844,
              "the plain reference");
static_assert(NamedIn("board:1836,1837").Count == 2 && NamedIn("board:1836,1837").Items[1] == 1837,
              "the comma list one commit message writes when it closes a pair");
static_assert(NamedIn("board/1844_label.md").Holds(1844) && NamedIn("board/1845_x.md").Holds(1845),
              "a path names an item as unambiguously as a reference does");
static_assert(NamedIn("the corpus is 2528 MB and an hour is 3600 s").Count == 0,
              "prose full of measurements names no item: a four-digit run is a reference only "
              "behind a marker (board:1846)");
static_assert(NamedIn("board:18 and the rest").Count == 0,
              "a marker followed by fewer than four digits names nothing -- taking four bytes "
              "after one digit yields garbage (board:1846)");
static_assert(NamedIn("board:18446").Count == 0,
              "and five digits are not four: the board's numbers are four wide, and a helper "
              "that takes the first four of five names an item that was never written");
static_assert(NamedIn("board:1844 closes what board/1845_x.md opened").Count == 2,
              "the two spellings in one message");

} // namespace outshine::Test::Board
