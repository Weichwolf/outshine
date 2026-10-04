#include "CssColour.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

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

constexpr std::array<NamedColour, 149> kColours = {{
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
    {.Spelling = "aliceblue", .Rgba = 0xF0F8FFFF},
    {.Spelling = "antiquewhite", .Rgba = 0xFAEBD7FF},
    {.Spelling = "aquamarine", .Rgba = 0x7FFFD4FF},
    {.Spelling = "azure", .Rgba = 0xF0FFFFFF},
    {.Spelling = "bisque", .Rgba = 0xFFE4C4FF},
    {.Spelling = "blanchedalmond", .Rgba = 0xFFEBCDFF},
    {.Spelling = "blueviolet", .Rgba = 0x8A2BE2FF},
    {.Spelling = "burlywood", .Rgba = 0xDEB887FF},
    {.Spelling = "cadetblue", .Rgba = 0x5F9EA0FF},
    {.Spelling = "chartreuse", .Rgba = 0x7FFF00FF},
    {.Spelling = "cornflowerblue", .Rgba = 0x6495EDFF},
    {.Spelling = "cornsilk", .Rgba = 0xFFF8DCFF},
    {.Spelling = "darkcyan", .Rgba = 0x008B8BFF},
    {.Spelling = "darkgoldenrod", .Rgba = 0xB8860BFF},
    {.Spelling = "darkkhaki", .Rgba = 0xBDB76BFF},
    {.Spelling = "darkmagenta", .Rgba = 0x8B008BFF},
    {.Spelling = "darkolivegreen", .Rgba = 0x556B2FFF},
    {.Spelling = "darkorange", .Rgba = 0xFF8C00FF},
    {.Spelling = "darkorchid", .Rgba = 0x9932CCFF},
    {.Spelling = "darksalmon", .Rgba = 0xE9967AFF},
    {.Spelling = "darkseagreen", .Rgba = 0x8FBC8FFF},
    {.Spelling = "darkslateblue", .Rgba = 0x483D8BFF},
    {.Spelling = "darkslategray", .Rgba = 0x2F4F4FFF},
    {.Spelling = "darkslategrey", .Rgba = 0x2F4F4FFF},
    {.Spelling = "darkturquoise", .Rgba = 0x00CED1FF},
    {.Spelling = "darkviolet", .Rgba = 0x9400D3FF},
    {.Spelling = "deeppink", .Rgba = 0xFF1493FF},
    {.Spelling = "deepskyblue", .Rgba = 0x00BFFFFF},
    {.Spelling = "dodgerblue", .Rgba = 0x1E90FFFF},
    {.Spelling = "floralwhite", .Rgba = 0xFFFAF0FF},
    {.Spelling = "ghostwhite", .Rgba = 0xF8F8FFFF},
    {.Spelling = "greenyellow", .Rgba = 0xADFF2FFF},
    {.Spelling = "honeydew", .Rgba = 0xF0FFF0FF},
    {.Spelling = "indianred", .Rgba = 0xCD5C5CFF},
    {.Spelling = "ivory", .Rgba = 0xFFFFF0FF},
    {.Spelling = "lavenderblush", .Rgba = 0xFFF0F5FF},
    {.Spelling = "lawngreen", .Rgba = 0x7CFC00FF},
    {.Spelling = "lemonchiffon", .Rgba = 0xFFFACDFF},
    {.Spelling = "lightcoral", .Rgba = 0xF08080FF},
    {.Spelling = "lightgoldenrodyellow", .Rgba = 0xFAFAD2FF},
    {.Spelling = "lightsalmon", .Rgba = 0xFFA07AFF},
    {.Spelling = "lightseagreen", .Rgba = 0x20B2AAFF},
    {.Spelling = "lightskyblue", .Rgba = 0x87CEFAFF},
    {.Spelling = "lightslategray", .Rgba = 0x778899FF},
    {.Spelling = "lightslategrey", .Rgba = 0x778899FF},
    {.Spelling = "lightsteelblue", .Rgba = 0xB0C4DEFF},
    {.Spelling = "limegreen", .Rgba = 0x32CD32FF},
    {.Spelling = "linen", .Rgba = 0xFAF0E6FF},
    {.Spelling = "mediumaquamarine", .Rgba = 0x66CDAAFF},
    {.Spelling = "mediumblue", .Rgba = 0x0000CDFF},
    {.Spelling = "mediumorchid", .Rgba = 0xBA55D3FF},
    {.Spelling = "mediumpurple", .Rgba = 0x9370DBFF},
    {.Spelling = "mediumseagreen", .Rgba = 0x3CB371FF},
    {.Spelling = "mediumslateblue", .Rgba = 0x7B68EEFF},
    {.Spelling = "mediumspringgreen", .Rgba = 0x00FA9AFF},
    {.Spelling = "mediumturquoise", .Rgba = 0x48D1CCFF},
    {.Spelling = "mediumvioletred", .Rgba = 0xC71585FF},
    {.Spelling = "mintcream", .Rgba = 0xF5FFFAFF},
    {.Spelling = "mistyrose", .Rgba = 0xFFE4E1FF},
    {.Spelling = "moccasin", .Rgba = 0xFFE4B5FF},
    {.Spelling = "navajowhite", .Rgba = 0xFFDEADFF},
    {.Spelling = "oldlace", .Rgba = 0xFDF5E6FF},
    {.Spelling = "olivedrab", .Rgba = 0x6B8E23FF},
    {.Spelling = "orangered", .Rgba = 0xFF4500FF},
    {.Spelling = "palegoldenrod", .Rgba = 0xEEE8AAFF},
    {.Spelling = "palegreen", .Rgba = 0x98FB98FF},
    {.Spelling = "paleturquoise", .Rgba = 0xAFEEEEFF},
    {.Spelling = "palevioletred", .Rgba = 0xDB7093FF},
    {.Spelling = "peru", .Rgba = 0xCD853FFF},
    {.Spelling = "powderblue", .Rgba = 0xB0E0E6FF},
    {.Spelling = "rebeccapurple", .Rgba = 0x663399FF},
    {.Spelling = "rosybrown", .Rgba = 0xBC8F8FFF},
    {.Spelling = "saddlebrown", .Rgba = 0x8B4513FF},
    {.Spelling = "sandybrown", .Rgba = 0xF4A460FF},
    {.Spelling = "seashell", .Rgba = 0xFFF5EEFF},
    {.Spelling = "sienna", .Rgba = 0xA0522DFF},
    {.Spelling = "slateblue", .Rgba = 0x6A5ACDFF},
    {.Spelling = "snow", .Rgba = 0xFFFAFAFF},
    {.Spelling = "springgreen", .Rgba = 0x00FF7FFF},
    {.Spelling = "thistle", .Rgba = 0xD8BFD8FF},
    {.Spelling = "tomato", .Rgba = 0xFF6347FF},
    {.Spelling = "wheat", .Rgba = 0xF5DEB3FF},
    {.Spelling = "yellowgreen", .Rgba = 0x9ACD32FF},
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
