#ifndef OUTSHINE_BASE_FORMAT_XMLATTRIBUTE_H
#define OUTSHINE_BASE_FORMAT_XMLATTRIBUTE_H

#include "Utf8.h"
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace outshine {

[[nodiscard]] constexpr bool XmlCharacter(uint32_t code) noexcept {
  constexpr uint32_t firstPrintable = 0x20;
  constexpr uint32_t lastBeforeSurrogates = 0xD7FF;
  constexpr uint32_t firstAfterSurrogates = 0xE000;
  constexpr uint32_t lastBmp = 0xFFFD;
  constexpr uint32_t lastUnicode = 0x10FFFF;
  return code == '\t' || code == '\n' || code == '\r' ||
         (code >= firstPrintable && code <= lastBeforeSurrogates) ||
         (code >= firstAfterSurrogates && code <= lastBmp) ||
         (code >= kThreeByteOver && code <= lastUnicode);
}

[[nodiscard]] inline std::optional<uint32_t> XmlReference(std::string_view name) {
  constexpr std::array<std::pair<std::string_view, uint32_t>, 5> predefined = {
      {{"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''}}};
  for (const auto &[spelling, code] : predefined) {
    if (name == spelling) { return code; }
  }
  if (!name.starts_with('#')) { return std::nullopt; }
  name.remove_prefix(1);
  constexpr int decimal = 10;
  constexpr int hexadecimal = 16;
  const bool hex = name.starts_with('x');
  if (hex) { name.remove_prefix(1); }
  uint32_t code = 0;
  const auto parsed =
      std::from_chars(name.data(), name.data() + name.size(), code, hex ? hexadecimal : decimal);
  if (parsed.ec != std::errc{} || parsed.ptr != name.data() + name.size() || !XmlCharacter(code)) {
    return std::nullopt;
  }
  return code;
}

[[nodiscard]] inline std::optional<std::string> DecodeXmlAttribute(std::string_view text) {
  std::string decoded;
  decoded.reserve(text.size());
  for (size_t at = 0; at < text.size(); ++at) {
    const char character = text[at];
    if (character == '&') {
      const size_t end = text.find(';', at + 1);
      if (end == std::string_view::npos) { return std::nullopt; }
      const auto code = XmlReference(text.substr(at + 1, end - at - 1));
      if (!code) { return std::nullopt; }
      AppendUtf8(decoded, *code);
      at = end;
    } else if (character == '\r') {
      decoded += ' ';
      if (at + 1 < text.size() && text[at + 1] == '\n') { ++at; }
    } else if (character == '\n' || character == '\t') {
      decoded += ' ';
    } else {
      if (character == '<' || !XmlCharacter(static_cast<unsigned char>(character))) {
        return std::nullopt;
      }
      decoded += character;
    }
  }
  return decoded;
}

}
#endif
