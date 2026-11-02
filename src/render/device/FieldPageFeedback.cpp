#include "FieldPageFeedback.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <span>

namespace outshine::Render {
namespace {
constexpr uint32_t kLowMask = 0x00ffffffu;
constexpr uint32_t kValid = 1u << 21u;
constexpr uint32_t kMipMask = 31;
constexpr uint32_t kLowBits = 24;
constexpr uint32_t kHighMask = 0xffu;

bool Word(float value, uint32_t high) noexcept {
  return std::isfinite(value) && value >= 0 && value < static_cast<float>(high) &&
         std::trunc(value) == value;
}
}

Native::Result<FieldPageFeedbackPixel> EncodeFieldPageFeedback(FieldPageAddress address) {
  if (address.Mip > kMipMask) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "field page feedback mip exceeds packet range"});
  }
  const auto x = std::bit_cast<uint32_t>(address.X);
  const auto y = std::bit_cast<uint32_t>(address.Y);
  const uint32_t high = (x >> kLowBits) | ((y >> kLowBits) << 8u) | (address.Mip << 16u) | kValid;
  return FieldPageFeedbackPixel{.Words = {static_cast<float>(x & kLowMask),
                                          static_cast<float>(y & kLowMask),
                                          static_cast<float>(high),
                                          0}};
}

Native::Result<std::optional<FieldPageAddress>>
DecodeFieldPageFeedback(FieldPageFeedbackPixel pixel) {
  const auto &words = pixel.Words;
  if (words == std::array<float, 4>{}) { return std::optional<FieldPageAddress>{}; }
  if (!Word(words[0], kLowMask + 1) || !Word(words[1], kLowMask + 1) ||
      !Word(words[2], kValid * 2) || words[2] < static_cast<float>(kValid) || words[3] != 0) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid field page feedback packet"});
  }
  const auto x = static_cast<uint32_t>(words[0]);
  const auto y = static_cast<uint32_t>(words[1]);
  const auto high = static_cast<uint32_t>(words[2]);
  return std::optional{
      FieldPageAddress{.X = std::bit_cast<int32_t>(x | ((high & kHighMask) << kLowBits)),
                       .Y = std::bit_cast<int32_t>(y | (((high >> 8u) & kHighMask) << kLowBits)),
                       .Mip = (high >> 16u) & kMipMask}};
}

Native::Result<size_t> DecodeFieldPageFeedback(std::span<const std::byte> bytes,
                                               std::span<FieldPageAddress> into) {
  if (bytes.size() % sizeof(FieldPageFeedbackPixel) != 0 ||
      bytes.size() / sizeof(FieldPageFeedbackPixel) > into.size()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "field page feedback exceeds its decode workspace"});
  }
  size_t count = 0;
  for (size_t at = 0; at < bytes.size(); at += sizeof(FieldPageFeedbackPixel)) {
    FieldPageFeedbackPixel pixel;
    std::memcpy(&pixel, bytes.data() + at, sizeof(pixel));
    auto address = DecodeFieldPageFeedback(pixel);
    if (!address) { return std::unexpected(address.error()); }
    const auto &request = *address;
    if (request) { into[count++] = *request; }
  }
  return count;
}
}
