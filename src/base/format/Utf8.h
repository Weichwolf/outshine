#ifndef OUTSHINE_BASE_FORMAT_UTF8_H
#define OUTSHINE_BASE_FORMAT_UTF8_H

#include <cstdint>
#include <string>
#include <string_view>

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

constexpr uint32_t kTwoByteLeadMask = 0xE0u;
constexpr uint32_t kThreeByteLeadMask = 0xF0u;
constexpr uint32_t kFourByteLeadMask = 0xF8u;
constexpr uint32_t kTwoByteBits = 0x1Fu;
constexpr uint32_t kThreeByteBits = 0x0Fu;
constexpr uint32_t kFourByteBits = 0x07u;

inline size_t ReadUtf8(std::string_view text, size_t at, char32_t &code) {
  const auto lead = static_cast<unsigned char>(text[at]);
  size_t length = 1;
  code = lead;
  if ((lead & kTwoByteLeadMask) == kTwoByteMark) {
    length = 2;
    code = lead & kTwoByteBits;
  } else if ((lead & kThreeByteLeadMask) == kThreeByteMark) {
    length = 3;
    code = lead & kThreeByteBits;
  } else if ((lead & kFourByteLeadMask) == kFourByteMark) {
    length = 4;
    code = lead & kFourByteBits;
  }
  if (at + length > text.size()) { return text.size() - at; }
  for (size_t i = 1; i < length; ++i) {
    code = (code << kContinuationBits) |
           (static_cast<unsigned char>(text[at + i]) & kContinuationMask);
  }
  return length;
}

constexpr uint32_t kGrinHigh = 0xD83Du;
constexpr uint32_t kGrinLow = 0xDE00u;
constexpr uint32_t kGrinCodePoint = 0x1F600u;

static_assert(PairedSurrogates(kGrinHigh, kGrinLow) == kGrinCodePoint,
              "a surrogate pair folds to the code point the standard states");

} // namespace outshine
#endif
