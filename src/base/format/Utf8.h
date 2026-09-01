#ifndef OUTSHINE_BASE_FORMAT_UTF8_H
#define OUTSHINE_BASE_FORMAT_UTF8_H

#include <cstdint>
#include <string>

namespace outshine {

constexpr uint32_t kAsciiOver = 0x80u;
constexpr uint32_t kTwoByteOver = 0x800u;
constexpr uint32_t kThreeByteOver = 0x10000u;

constexpr uint32_t kContinuationMark = 0x80u;
constexpr uint32_t kTwoByteMark = 0xC0u;
constexpr uint32_t kThreeByteMark = 0xE0u;
constexpr uint32_t kFourByteMark = 0xF0u;
constexpr uint32_t kContinuationMask = 0x3Fu;
constexpr unsigned kContinuationBits = 6u;

constexpr uint32_t kSurrogateHighFirst = 0xD800u;
constexpr uint32_t kSurrogateHighLast = 0xDBFFu;
constexpr uint32_t kSurrogateLowFirst = 0xDC00u;
constexpr uint32_t kSurrogateLowLast = 0xDFFFu;
constexpr uint32_t kReplacement = 0xFFFDu;

[[nodiscard]] constexpr bool IsHighSurrogate(uint32_t code) {
  return code >= kSurrogateHighFirst && code <= kSurrogateHighLast;
}

[[nodiscard]] constexpr bool IsLowSurrogate(uint32_t code) {
  return code >= kSurrogateLowFirst && code <= kSurrogateLowLast;
}

[[nodiscard]] constexpr bool IsSurrogate(uint32_t code) {
  return code >= kSurrogateHighFirst && code <= kSurrogateLowLast;
}

[[nodiscard]] constexpr uint32_t PairedSurrogates(uint32_t high, uint32_t low) {
  return kThreeByteOver + ((high - kSurrogateHighFirst) << 10u) + (low - kSurrogateLowFirst);
}

inline void AppendUtf8(std::string &out, uint32_t code) {
  const auto tail = [&out](uint32_t bits) {
    out.push_back(static_cast<char>(kContinuationMark | (bits & kContinuationMask)));
  };
  if (code < kAsciiOver) {
    out.push_back(static_cast<char>(code));
  } else if (code < kTwoByteOver) {
    out.push_back(static_cast<char>(kTwoByteMark | (code >> kContinuationBits)));
    tail(code);
  } else if (code < kThreeByteOver) {
    out.push_back(static_cast<char>(kThreeByteMark | (code >> (2u * kContinuationBits))));
    tail(code >> kContinuationBits);
    tail(code);
  } else {
    out.push_back(static_cast<char>(kFourByteMark | (code >> (3u * kContinuationBits))));
    tail(code >> (2u * kContinuationBits));
    tail(code >> kContinuationBits);
    tail(code);
  }
}

static_assert(PairedSurrogates(0xD83Du, 0xDE00u) == 0x1F600u,
              "a surrogate pair folds to the code point the standard states");

} // namespace outshine
#endif
