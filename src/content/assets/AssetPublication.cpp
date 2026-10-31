#include "AssetPublication.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace outshine {
std::optional<AssetRequestKey> AssetRequestKey::Parse(std::string_view hex) noexcept {
  const auto key = AssetProductKey::Parse(hex);
  return key ? std::optional(AssetRequestKey{key->Digest}) : std::nullopt;
}

std::optional<AssetProductKey> AssetProductKey::Parse(std::string_view hex) noexcept {
  constexpr size_t kHexDigits = 64;
  constexpr uint8_t kRadix = 16;
  if (hex.size() != kHexDigits) { return std::nullopt; }
  const auto digit = [](char c) -> std::optional<uint8_t> {
    if (c >= '0' && c <= '9') { return static_cast<uint8_t>(c - '0'); }
    if (c >= 'a' && c <= 'f') { return static_cast<uint8_t>(c - 'a' + 10); }
    return std::nullopt;
  };
  AssetProductKey key;
  for (size_t i = 0; i < key.Digest.size(); ++i) {
    const auto high = digit(hex[2 * i]);
    const auto low = digit(hex[2 * i + 1]);
    if (!high || !low) { return std::nullopt; }
    key.Digest[i] = static_cast<uint8_t>(*high * kRadix + *low);
  }
  return key;
}
}
