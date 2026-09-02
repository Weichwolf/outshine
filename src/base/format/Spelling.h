#ifndef OUTSHINE_BASE_FORMAT_SPELLING_H
#define OUTSHINE_BASE_FORMAT_SPELLING_H

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace outshine {

template <typename Held> using Spelling = std::pair<std::string_view, Held>;

template <typename Held, size_t N> using Spellings = std::array<Spelling<Held>, N>;

template <typename Held, size_t N>
[[nodiscard]] constexpr bool EverySpellingStandsOnce(const Spellings<Held, N> &table) {
  for (size_t at = 0; at < N; ++at) {
    if (table[at].first.empty()) { return false; }
    for (size_t over = at + 1; over < N; ++over) {
      if (table[at].first == table[over].first) { return false; }
    }
  }
  return true;
}

template <typename Held, size_t N>
[[nodiscard]] constexpr Held
Means(const Spellings<Held, N> &table, std::string_view said, Held unsaid) {
  for (const Spelling<Held> &one : table) {
    if (one.first == said) { return one.second; }
  }
  return unsaid;
}

template <typename Held, size_t N>
[[nodiscard]] constexpr std::string_view
SpellingOf(const Spellings<Held, N> &table, Held held, std::string_view unsaid) {
  for (const Spelling<Held> &one : table) {
    if (one.second == held) { return one.first; }
  }
  return unsaid;
}

} // namespace outshine

#endif
