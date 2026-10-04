#include "CssColour.h"

#include <algorithm>
#include <array>

namespace outshine {
namespace {
constexpr int kOpaque = 255;
constexpr int kHexPlace = 16;
constexpr int kNibbleToByte = 17;
constexpr unsigned kRedShift = 24u;
constexpr unsigned kGreenShift = 16u;
constexpr unsigned kBlueShift = 8u;

struct NamedColour {
  std::string_view Spelling;
  uint32_t Rgba;
};

constexpr std::array<NamedColour, 66> kColours = {{
    {.Spelling = "transparent", .Rgba = 0x00000000},
    {.Spelling = "black", .Rgba = 0x000000FF},
    {.Spelling = "white", .Rgba = 0xFFFFFFFF},
    {.Spelling = "red", .Rgba = 0xFF0000FF},
    {.Spelling = "green", .Rgba = 0x008000FF},
    {.Spelling = "blue", .Rgba = 0x0000FFFF},
    {.Spelling = "yellow", .Rgba = 0xFFFF00FF},
    {.Spelling = "orange", .Rgba = 0xFFA500FF},
    {.Spelling = "purple", .Rgba = 0x800080FF},
    {.Spelling = "gray", .Rgba = 0x808080FF},
    {.Spelling = "grey", .Rgba = 0x808080FF},
    {.Spelling = "silver", .Rgba = 0xC0C0C0FF},
    {.Spelling = "lightgray", .Rgba = 0xD3D3D3FF},
    {.Spelling = "lightgrey", .Rgba = 0xD3D3D3FF},
    {.Spelling = "lightgreen", .Rgba = 0x90EE90FF},
    {.Spelling = "pink", .Rgba = 0xFFC0CBFF},
    {.Spelling = "teal", .Rgba = 0x008080FF},
    {.Spelling = "navy", .Rgba = 0x000080FF},
    {.Spelling = "lime", .Rgba = 0x00FF00FF},
    {.Spelling = "aqua", .Rgba = 0x00FFFFFF},
    {.Spelling = "fuchsia", .Rgba = 0xFF00FFFF},
    {.Spelling = "maroon", .Rgba = 0x800000FF},
    {.Spelling = "olive", .Rgba = 0x808000FF},
    {.Spelling = "lightblue", .Rgba = 0xADD8E6FF},
    {.Spelling = "salmon", .Rgba = 0xFA8072FF},
    {.Spelling = "cyan", .Rgba = 0x00FFFFFF},
    {.Spelling = "magenta", .Rgba = 0xFF00FFFF},
    {.Spelling = "brown", .Rgba = 0xA52A2AFF},
    {.Spelling = "gold", .Rgba = 0xFFD700FF},
    {.Spelling = "violet", .Rgba = 0xEE82EEFF},
    {.Spelling = "indigo", .Rgba = 0x4B0082FF},
    {.Spelling = "beige", .Rgba = 0xF5F5DCFF},
    {.Spelling = "tan", .Rgba = 0xD2B48CFF},
    {.Spelling = "coral", .Rgba = 0xFF7F50FF},
    {.Spelling = "khaki", .Rgba = 0xF0E68CFF},
    {.Spelling = "plum", .Rgba = 0xDDA0DDFF},
    {.Spelling = "orchid", .Rgba = 0xDA70D6FF},
    {.Spelling = "skyblue", .Rgba = 0x87CEEBFF},
    {.Spelling = "steelblue", .Rgba = 0x4682B4FF},
    {.Spelling = "darkgray", .Rgba = 0xA9A9A9FF},
    {.Spelling = "darkgrey", .Rgba = 0xA9A9A9FF},
    {.Spelling = "lightyellow", .Rgba = 0xFFFFE0FF},
    {.Spelling = "lightpink", .Rgba = 0xFFB6C1FF},
    {.Spelling = "lightcyan", .Rgba = 0xE0FFFFFF},
    {.Spelling = "seagreen", .Rgba = 0x2E8B57FF},
    {.Spelling = "darkblue", .Rgba = 0x00008BFF},
    {.Spelling = "darkgreen", .Rgba = 0x006400FF},
    {.Spelling = "darkred", .Rgba = 0x8B0000FF},
    {.Spelling = "hotpink", .Rgba = 0xFF69B4FF},
    {.Spelling = "papayawhip", .Rgba = 0xFFEFD5FF},
    {.Spelling = "whitesmoke", .Rgba = 0xF5F5F5FF},
    {.Spelling = "gainsboro", .Rgba = 0xDCDCDCFF},
    {.Spelling = "peachpuff", .Rgba = 0xFFDAB9FF},
    {.Spelling = "lavender", .Rgba = 0xE6E6FAFF},
    {.Spelling = "turquoise", .Rgba = 0x40E0D0FF},
    {.Spelling = "crimson", .Rgba = 0xDC143CFF},
    {.Spelling = "chocolate", .Rgba = 0xD2691EFF},
    {.Spelling = "goldenrod", .Rgba = 0xDAA520FF},
    {.Spelling = "firebrick", .Rgba = 0xB22222FF},
    {.Spelling = "forestgreen", .Rgba = 0x228B22FF},
    {.Spelling = "midnightblue", .Rgba = 0x191970FF},
    {.Spelling = "royalblue", .Rgba = 0x4169E1FF},
    {.Spelling = "slategray", .Rgba = 0x708090FF},
    {.Spelling = "slategrey", .Rgba = 0x708090FF},
    {.Spelling = "dimgray", .Rgba = 0x696969FF},
    {.Spelling = "dimgrey", .Rgba = 0x696969FF},
}};

int HexOf(char c) {
  if (c >= '0' && c <= '9') { return c - '0'; }
  if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
  if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
  return -1;
}

bool ReadHexColour(std::string_view digits, uint32_t &out) {
  std::array<int, 4> channel = {{0, 0, 0, kOpaque}};
  if (digits.size() == 3 || digits.size() == 4) {
    for (size_t at = 0; at < digits.size(); ++at) {
      const int one = HexOf(digits[at]);
      if (one < 0) { return false; }
      channel[at] = one * kNibbleToByte;
    }
  } else if (digits.size() == 6 || digits.size() == 8) {
    for (size_t at = 0; at + 1 < digits.size(); at += 2) {
      const int high = HexOf(digits[at]);
      const int low = HexOf(digits[at + 1]);
      if (high < 0 || low < 0) { return false; }
      channel[at / 2] = high * kHexPlace + low;
    }
  } else {
    return false;
  }
  out = (static_cast<uint32_t>(channel[0]) << kRedShift) |
        (static_cast<uint32_t>(channel[1]) << kGreenShift) |
        (static_cast<uint32_t>(channel[2]) << kBlueShift) | static_cast<uint32_t>(channel[3]);
  return true;
}

}

std::optional<uint32_t> ParseCssColour(std::string_view text) noexcept {
  const auto space = [](char c) { return c == ' ' || (c >= '\t' && c <= '\r'); };
  while (!text.empty() && space(text.front())) { text.remove_prefix(1); }
  while (!text.empty() && space(text.back())) { text.remove_suffix(1); }
  uint32_t rgba = 0;
  if (!text.empty() && text.front() == '#') {
    return ReadHexColour(text.substr(1), rgba) ? std::optional{rgba} : std::nullopt;
  }
  const auto *const named = std::ranges::find_if(kColours, [text](const NamedColour &one) {
    return std::ranges::equal(text, one.Spelling, [](char a, char b) {
      return (a >= 'A' && a <= 'Z' ? static_cast<char>(a - 'A' + 'a') : a) == b;
    });
  });
  return named == kColours.end() ? std::nullopt : std::optional{named->Rgba};
}
}
