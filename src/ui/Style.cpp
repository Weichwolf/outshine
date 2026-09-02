#include <array>
#include <charconv>
#include "Style.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine::Ui {

constexpr int kOpaque = 255;
constexpr int kNibbleToByte = 17;
constexpr unsigned kRedShift = 24u;
constexpr unsigned kGreenShift = 16u;
constexpr unsigned kBlueShift = 8u;
constexpr int kIdWeight = 10000;
constexpr int kClassWeight = 100;

namespace {

bool Space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

std::string Lower(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
  }
  return out;
}

std::string_view Trim(std::string_view text) {
  while (!text.empty() && Space(text.front())) { text.remove_prefix(1); }
  while (!text.empty() && Space(text.back())) { text.remove_suffix(1); }
  return text;
}

struct Named {
  const char *Spelling;
  Property What;
};

const std::array<Named, 48> kProperties = {{
    {.Spelling = "display", .What = Property::Display},
    {.Spelling = "position", .What = Property::Position},
    {.Spelling = "box-sizing", .What = Property::BoxSizing},
    {.Spelling = "overflow", .What = Property::Overflow},
    {.Spelling = "flex-direction", .What = Property::FlexDirection},
    {.Spelling = "flex-wrap", .What = Property::FlexWrap},
    {.Spelling = "justify-content", .What = Property::JustifyContent},
    {.Spelling = "align-items", .What = Property::AlignItems},
    {.Spelling = "align-self", .What = Property::AlignSelf},
    {.Spelling = "align-content", .What = Property::AlignContent},
    {.Spelling = "flex-grow", .What = Property::FlexGrow},
    {.Spelling = "flex-shrink", .What = Property::FlexShrink},
    {.Spelling = "flex-basis", .What = Property::FlexBasis},
    {.Spelling = "gap", .What = Property::Gap},
    {.Spelling = "row-gap", .What = Property::RowGap},
    {.Spelling = "column-gap", .What = Property::ColumnGap},
    {.Spelling = "width", .What = Property::Width},
    {.Spelling = "height", .What = Property::Height},
    {.Spelling = "min-width", .What = Property::MinWidth},
    {.Spelling = "max-width", .What = Property::MaxWidth},
    {.Spelling = "min-height", .What = Property::MinHeight},
    {.Spelling = "max-height", .What = Property::MaxHeight},
    {.Spelling = "margin-top", .What = Property::MarginTop},
    {.Spelling = "margin-right", .What = Property::MarginRight},
    {.Spelling = "margin-bottom", .What = Property::MarginBottom},
    {.Spelling = "margin-left", .What = Property::MarginLeft},
    {.Spelling = "padding-top", .What = Property::PaddingTop},
    {.Spelling = "padding-right", .What = Property::PaddingRight},
    {.Spelling = "padding-bottom", .What = Property::PaddingBottom},
    {.Spelling = "padding-left", .What = Property::PaddingLeft},
    {.Spelling = "border-top-width", .What = Property::BorderTopWidth},
    {.Spelling = "border-right-width", .What = Property::BorderRightWidth},
    {.Spelling = "border-bottom-width", .What = Property::BorderBottomWidth},
    {.Spelling = "border-left-width", .What = Property::BorderLeftWidth},
    {.Spelling = "top", .What = Property::Top},
    {.Spelling = "right", .What = Property::Right},
    {.Spelling = "bottom", .What = Property::Bottom},
    {.Spelling = "left", .What = Property::Left},
    {.Spelling = "background-color", .What = Property::BackgroundColour},
    {.Spelling = "border-color", .What = Property::BorderColour},
    {.Spelling = "border-radius", .What = Property::BorderRadius},
    {.Spelling = "opacity", .What = Property::Opacity},
    {.Spelling = "color", .What = Property::Colour},
    {.Spelling = "font-size", .What = Property::FontSize},
    {.Spelling = "line-height", .What = Property::LineHeight},
    {.Spelling = "text-align", .What = Property::TextAlign},
    {.Spelling = "white-space", .What = Property::WhiteSpace},
    {.Spelling = "font-family", .What = Property::FontFamily},
}};

struct NamedColour {
  const char *Spelling;
  uint32_t Rgba;
};

const std::array<NamedColour, 66> kColours = {{
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

bool ReadColour(std::string_view text, uint32_t &out) {
  if (!text.empty() && text[0] == '#') {
    const std::string_view digits = text.substr(1);
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
        channel[at / 2] = high * 16 + low;
      }
    } else {
      return false;
    }
    out = (static_cast<uint32_t>(channel[0]) << kRedShift) |
          (static_cast<uint32_t>(channel[1]) << kGreenShift) |
          (static_cast<uint32_t>(channel[2]) << kBlueShift) | static_cast<uint32_t>(channel[3]);
    return true;
  }
  const std::string lowered = Lower(text);
  for (const NamedColour &one : kColours) {
    if (lowered == one.Spelling) {
      out = one.Rgba;
      return true;
    }
  }
  return false;
}

Value ReadValue(std::string_view text) {
  const std::string_view trimmed = Trim(text);
  Value value;
  if (trimmed.empty()) { return value; }

  value.Prefixed = trimmed.front() == '-';
  if (trimmed == "auto") {
    value.How = Unit::Auto;
    return value;
  }
  uint32_t rgba = 0;
  if (ReadColour(trimmed, rgba)) {
    value.How = Unit::Colour;
    value.Word = rgba;
    return value;
  }
  const char first = trimmed.front();
  if ((first >= '0' && first <= '9') || first == '-' || first == '+' || first == '.') {
    const std::string_view digits = trimmed.front() == '+' ? trimmed.substr(1) : trimmed;
    double number = 0.0;
    const auto scanned = std::from_chars(digits.data(), digits.data() + digits.size(), number);
    const std::string_view suffix =
        Trim(digits.substr(static_cast<size_t>(scanned.ptr - digits.data())));
    value.Number = number;
    if (suffix.empty()) {
      value.How = Unit::None;
    } else if (suffix == "px") {
      value.How = Unit::Pixels;
    } else if (suffix == "%") {
      value.How = Unit::Percent;
    } else if (suffix == "em") {
      value.How = Unit::Em;
    } else if (suffix == "rem") {
      value.How = Unit::Rem;
    } else {
      value.How = Unit::Keyword;
      value.Word = Keyword(trimmed);
    }
    return value;
  }
  value.How = Unit::Keyword;
  value.Word = Keyword(trimmed);
  return value;
}

} // namespace

Property PropertyNamed(std::string_view name) {
  const std::string lowered = Lower(name);
  for (const Named &one : kProperties) {
    if (lowered == one.Spelling) { return one.What; }
  }
  return Property::kCount;
}

struct Vocabulary {
  Property What;
  const std::array<const char *, 14> Words;
};

const std::array<Vocabulary, 12> kVocabularies = {{
    {.What = Property::Display,
     .Words = {"block", "flex", "inline", "none", "inline-flex", nullptr}},
    {.What = Property::Position, .Words = {"static", "relative", nullptr}},
    {.What = Property::BoxSizing, .Words = {"content-box", "border-box", nullptr}},
    {.What = Property::Overflow, .Words = {"visible", "hidden", "scroll", nullptr}},
    {.What = Property::FlexDirection,
     .Words = {"row", "column", "row-reverse", "column-reverse", nullptr}},
    {.What = Property::FlexWrap, .Words = {"nowrap", "wrap", "wrap-reverse", nullptr}},
    {.What = Property::JustifyContent,
     .Words = {"flex-start",
               "flex-end",
               "center",
               "space-between",
               "space-around",
               "space-evenly",
               "start",
               "end",
               "left",
               "right",
               "normal",
               "stretch",
               nullptr}},
    {.What = Property::AlignItems,
     .Words = {"stretch",
               "flex-start",
               "flex-end",
               "center",
               "start",
               "end",
               "baseline",
               "normal",
               nullptr}},
    {.What = Property::AlignSelf,
     .Words = {"auto",
               "stretch",
               "flex-start",
               "flex-end",
               "center",
               "start",
               "end",
               "baseline",
               "normal",
               nullptr}},
    {.What = Property::AlignContent,
     .Words = {"stretch",
               "flex-start",
               "flex-end",
               "center",
               "space-between",
               "space-around",
               "space-evenly",
               "start",
               "end",
               "normal",
               nullptr}},
    {.What = Property::TextAlign, .Words = {"left", "right", "center", nullptr}},
    {.What = Property::WhiteSpace, .Words = {"normal", "pre", nullptr}},
}};

namespace {

[[nodiscard]] bool WordIsHeld(Property what, const Value &value) {
  if (value.How != Unit::Keyword) { return true; }

  if (value.Prefixed) { return true; }
  for (const Vocabulary &vocabulary : kVocabularies) {
    if (vocabulary.What != what) { continue; }
    for (const char *word : vocabulary.Words) {
      if (word == nullptr) { break; }
      if (Keyword(word) == value.Word) { return true; }
    }
    return false;
  }
  return true;
}
} // namespace

namespace {

bool Bare(std::string_view text) {
  const Value value = ReadValue(text);
  return value.How == Unit::None;
}

void Expand(std::string_view name,
            std::string_view text,
            std::vector<Declaration> &into,
            size_t &unheld,
            std::vector<std::string> &names) {
  const std::string lowered = Lower(name);

  if (!lowered.empty() && lowered.front() == '-') { return; }
  const auto one = [&](Property what, std::string_view value) {
    into.push_back({.What = what, .How = ReadValue(value)});
  };
  const auto words = [&] {
    std::vector<std::string_view> parts;
    size_t at = 0;
    while (at < text.size()) {
      while (at < text.size() && Space(text[at])) { ++at; }
      const size_t from = at;
      while (at < text.size() && !Space(text[at])) { ++at; }
      if (at > from) { parts.push_back(text.substr(from, at - from)); }
    }
    return parts;
  };

  const auto sides = [&](Property top, Property right, Property bottom, Property left) {
    const std::vector<std::string_view> parts = words();
    if (parts.empty() || parts.size() > 4) { return false; }
    const std::string_view t = parts[0];
    const std::string_view r = parts.size() > 1 ? parts[1] : t;
    const std::string_view b = parts.size() > 2 ? parts[2] : t;
    const std::string_view l = parts.size() > 3 ? parts[3] : r;
    one(top, t);
    one(right, r);
    one(bottom, b);
    one(left, l);
    return true;
  };

  const auto flex = [&] {
    const std::vector<std::string_view> parts = words();
    if (parts.empty() || parts.size() > 3) { return false; }
    if (parts.size() == 1 && (parts[0] == "none" || parts[0] == "auto" || parts[0] == "initial")) {
      one(Property::FlexGrow, parts[0] == "auto" ? "1" : "0");
      one(Property::FlexShrink, parts[0] == "none" ? "0" : "1");
      one(Property::FlexBasis, "auto");
      return true;
    }
    size_t numbers = 0;
    size_t basisAt = parts.size();
    std::array<std::string_view, 2> bare{};
    for (size_t at = 0; at < parts.size(); ++at) {
      if (Bare(parts[at])) {
        if (numbers >= 2) { return true; }
        bare[numbers++] = parts[at];
      } else if (basisAt == parts.size()) {
        basisAt = at;
      } else {
        return true;
      }
    }
    if (numbers == 0) {
      if (basisAt == parts.size()) { return false; }
      one(Property::FlexGrow, "1");
      one(Property::FlexShrink, "1");
      one(Property::FlexBasis, parts[basisAt]);
      return true;
    }

    if (numbers == 2 && basisAt < parts.size()) {
      size_t first = parts.size();
      size_t second = parts.size();
      for (size_t at = 0; at < parts.size(); ++at) {
        if (!Bare(parts[at])) { continue; }
        if (first == parts.size()) {
          first = at;
        } else {
          second = at;
        }
      }
      if (basisAt > first && basisAt < second) { return true; }
    }
    one(Property::FlexGrow, bare[0]);
    one(Property::FlexShrink, numbers > 1 ? bare[1] : std::string_view("1"));

    one(Property::FlexBasis, basisAt < parts.size() ? parts[basisAt] : std::string_view("0%"));
    return true;
  };
  const auto flexFlow = [&] {
    const std::vector<std::string_view> parts = words();
    if (parts.empty() || parts.size() > 2) { return false; }
    for (const std::string_view part : parts) {
      const bool wraps = part == "wrap" || part == "nowrap" || part == "wrap-reverse";
      one(wraps ? Property::FlexWrap : Property::FlexDirection, part);
    }
    return true;
  };

  const auto single = [&](Property what) {
    const std::vector<std::string_view> parts = words();
    if (parts.size() != 1) { return false; }
    const Value value = ReadValue(parts[0]);
    if (value.How != Unit::Colour) { return false; }
    one(what, parts[0]);
    return true;
  };
  const auto border = [&](Property top, Property right, Property bottom, Property left) {
    const std::vector<std::string_view> parts = words();
    if (parts.empty() || parts.size() > 3) { return false; }
    std::string_view width = "medium";
    bool sawStyle = false;
    bool drawn = true;
    for (const std::string_view part : parts) {
      if (part == "none" || part == "hidden") {
        sawStyle = true;
        drawn = false;
      } else if (part == "solid") {
        sawStyle = true;
      } else if (ReadValue(part).How == Unit::Colour) {
        one(Property::BorderColour, part);
      } else if (part == "thin" || part == "medium" || part == "thick") {
        width = part == "thin" ? "1px" : part == "medium" ? "3px" : "5px";
      } else if (ReadValue(part).How == Unit::Pixels || ReadValue(part).How == Unit::Em ||
                 (ReadValue(part).How == Unit::None && ReadValue(part).Number == 0.0)) {
        width = part == "0" ? std::string_view("0px") : part;
      } else {
        return false;
      }
    }

    if (!sawStyle && parts.size() == 1 && ReadValue(parts[0]).How != Unit::Colour) {
      sawStyle = true;
      drawn = true;
    }
    if (!sawStyle) { return false; }
    const std::string_view used = drawn ? width : std::string_view("0");

    for (const Property side : {top, right, bottom, left}) {
      if (side != Property::kCount) { one(side, used); }
    }
    return true;
  };
  if (lowered == "margin") {
    if (sides(Property::MarginTop,
              Property::MarginRight,
              Property::MarginBottom,
              Property::MarginLeft)) {
      return;
    }
  } else if (lowered == "padding") {
    if (sides(Property::PaddingTop,
              Property::PaddingRight,
              Property::PaddingBottom,
              Property::PaddingLeft)) {
      return;
    }
  } else if (lowered == "border-width") {
    if (sides(Property::BorderTopWidth,
              Property::BorderRightWidth,
              Property::BorderBottomWidth,
              Property::BorderLeftWidth)) {
      return;
    }
  } else if (lowered == "flex") {
    if (flex()) { return; }
  } else if (lowered == "flex-flow") {
    if (flexFlow()) { return; }
  } else if (lowered == "background") {
    if (single(Property::BackgroundColour)) { return; }
  } else if (lowered == "border") {
    if (border(Property::BorderTopWidth,
               Property::BorderRightWidth,
               Property::BorderBottomWidth,
               Property::BorderLeftWidth)) {
      return;
    }
  } else if (lowered == "border-top") {
    if (border(Property::BorderTopWidth, Property::kCount, Property::kCount, Property::kCount)) {
      return;
    }
  } else if (lowered == "border-right") {
    if (border(Property::kCount, Property::BorderRightWidth, Property::kCount, Property::kCount)) {
      return;
    }
  } else if (lowered == "border-bottom") {
    if (border(Property::kCount, Property::kCount, Property::BorderBottomWidth, Property::kCount)) {
      return;
    }
  } else if (lowered == "border-left") {
    if (border(Property::kCount, Property::kCount, Property::kCount, Property::BorderLeftWidth)) {
      return;
    }
  } else {
    const Property what = PropertyNamed(lowered);
    if (what != Property::kCount) {
      const Value value = ReadValue(text);
      if (!WordIsHeld(what, value)) {
        ++unheld;
        if (names.size() < 64) { names.push_back(lowered + ":" + std::string(Trim(text))); }
        return;
      }
      into.push_back({.What = what, .How = value});
      return;
    }
  }
  ++unheld;

  if (names.size() < 64) { names.push_back(lowered + ":" + std::string(Trim(text))); }
}

std::string WithoutComments(std::string_view css) {
  std::string out;
  out.reserve(css.size());
  size_t at = 0;
  while (at < css.size()) {
    if (css.compare(at, 2, "/*") == 0) {
      const size_t end = css.find("*/", at + 2);
      at = end == std::string_view::npos ? css.size() : end + 2;
      continue;
    }
    out.push_back(css[at++]);
  }
  return out;
}

void ReadBlock(std::string_view body,
               std::vector<Declaration> &into,
               size_t &unheld,
               std::vector<std::string> &names) {
  size_t at = 0;
  while (at < body.size()) {
    const size_t semi = body.find(';', at);
    const std::string_view one =
        Trim(body.substr(at, semi == std::string_view::npos ? std::string_view::npos : semi - at));
    at = semi == std::string_view::npos ? body.size() : semi + 1;
    if (one.empty()) { continue; }
    const size_t colon = one.find(':');
    if (colon == std::string_view::npos) { continue; }
    Expand(Trim(one.substr(0, colon)), Trim(one.substr(colon + 1)), into, unheld, names);
  }
}

bool ReadCompound(std::string_view text, Compound &out) {
  size_t at = 0;

  const size_t nth = text.find(":nth-child(");
  if (nth != std::string_view::npos) {
    const size_t close = text.find(')', nth);
    if (close == std::string_view::npos) { return false; }
    const std::string_view inside = text.substr(nth + 11, close - nth - 11);
    if (close + 1 != text.size() || inside.empty()) { return false; }
    for (const char c : inside) {
      if (c < '0' || c > '9') { return false; }
    }
    out.NthChild = 0;
    (void)std::from_chars(inside.data(), inside.data() + inside.size(), out.NthChild);
    if (out.NthChild <= 0) { return false; }
    text = text.substr(0, nth);
    if (text.empty()) { return true; }
  }
  if (text == "*") {
    out.Universal = true;
    return true;
  }
  while (at < text.size()) {
    const char lead = text[at];
    if (lead == '.' || lead == '#') { ++at; }
    const size_t from = at;
    while (at < text.size() && text[at] != '.' && text[at] != '#') { ++at; }
    const std::string part = Lower(text.substr(from, at - from));
    if (part.empty()) { return false; }

    for (const char c : part) {
      if ((c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-' && c != '_') { return false; }
    }
    if (lead == '.') {
      out.Classes.push_back(part);
    } else if (lead == '#') {
      if (!out.Id.empty()) { return false; }
      out.Id = part;
    } else {
      if (!out.Tag.empty()) { return false; }
      out.Tag = part;
    }
  }
  return out.Universal || !out.Tag.empty() || !out.Classes.empty() || !out.Id.empty() ||
         out.NthChild != 0;
}

} // namespace

std::vector<Declaration> Stylesheet::Inline(std::string_view text) {
  std::vector<Declaration> out;
  ReadBlock(WithoutComments(text), out, Unheld_, Names_);
  return out;
}

void Stylesheet::Read(std::string_view text) {
  const std::string stripped = WithoutComments(text);
  const std::string_view css = stripped;
  size_t at = 0;
  while (at < css.size()) {
    if (Space(css[at])) {
      ++at;
      continue;
    }

    if (css[at] == '@') {
      ++Unselectable_;
      if (Names_.size() < 64) {
        const size_t stops = css.find_first_of("{; \t\r\n", at);
        Names_.emplace_back(
            css.substr(at, (stops == std::string_view::npos ? css.size() : stops) - at));
      }
      const size_t brace = css.find('{', at);
      const size_t semi = css.find(';', at);
      if (semi != std::string_view::npos && (brace == std::string_view::npos || semi < brace)) {
        at = semi + 1;
        continue;
      }
      if (brace == std::string_view::npos) { break; }
      int depth = 0;
      size_t cursor = brace;
      for (; cursor < css.size(); ++cursor) {
        if (css[cursor] == '{') { ++depth; }
        if (css[cursor] == '}' && --depth == 0) { break; }
      }
      at = cursor < css.size() ? cursor + 1 : css.size();
      continue;
    }
    const size_t brace = css.find('{', at);
    if (brace == std::string_view::npos) { break; }

    size_t close = brace;
    int depth = 0;
    for (; close < css.size(); ++close) {
      if (css[close] == '{') { ++depth; }
      if (css[close] == '}' && --depth == 0) { break; }
    }
    const std::string_view heads = css.substr(at, brace - at);
    const std::string_view body =
        css.substr(brace + 1, (close >= css.size() ? css.size() : close) - brace - 1);
    at = close >= css.size() ? css.size() : close + 1;

    std::vector<Declaration> declares;

    const size_t nested = body.find('{');
    if (nested != std::string_view::npos) {
      ++Unselectable_;
      if (Names_.size() < 64) { Names_.emplace_back("nested-rule"); }
    }
    ReadBlock(nested == std::string_view::npos ? body : body.substr(0, body.rfind(';', nested) + 1),
              declares,
              Unheld_,
              Names_);

    size_t from = 0;
    while (from <= heads.size()) {
      const size_t comma = heads.find(',', from);
      const std::string_view head = Trim(heads.substr(
          from, comma == std::string_view::npos ? std::string_view::npos : comma - from));
      from = comma == std::string_view::npos ? heads.size() + 1 : comma + 1;
      if (head.empty()) { continue; }
      Rule rule;
      bool held = true;
      Reach reach = Reach::Descendant;
      size_t cursor = 0;
      while (cursor < head.size() && held) {
        while (cursor < head.size() && Space(head[cursor])) { ++cursor; }

        if (cursor < head.size() && head[cursor] == '>') {
          reach = Reach::Child;
          ++cursor;
          continue;
        }
        const size_t start = cursor;
        while (cursor < head.size() && !Space(head[cursor]) && head[cursor] != '>') { ++cursor; }
        if (cursor == start) { break; }
        Compound compound;
        held = ReadCompound(head.substr(start, cursor - start), compound);
        if (held) {
          if (!rule.Chain.empty()) { rule.Links.push_back(reach); }
          reach = Reach::Descendant;
          rule.Chain.push_back(std::move(compound));
        }
      }
      if (!held || rule.Chain.empty()) {
        ++Unselectable_;
        if (Names_.size() < 64) { Names_.emplace_back(head); }
        continue;
      }
      for (const Compound &one : rule.Chain) {
        rule.Specificity += one.Id.empty() ? 0 : kIdWeight;

        rule.Specificity +=
            static_cast<int>(one.Classes.size() + (one.NthChild > 0 ? 1u : 0u)) * kClassWeight;
        rule.Specificity += one.Tag.empty() ? 0 : 1;
      }
      rule.Order = Order_++;
      rule.Declares = declares;
      Rules_.push_back(std::move(rule));
    }
  }
}

namespace {

bool Holds(const Compound &compound, const Markup &markup, int node) {
  const Node &element = markup.Nodes()[static_cast<size_t>(node)];
  if (element.Kind != NodeKind::Element) { return false; }
  if (!compound.Tag.empty() && element.Name != compound.Tag) { return false; }
  if (compound.NthChild > 0) {
    if (element.Parent < 0) { return false; }
    int position = 0;
    for (const int sibling : markup.Nodes()[static_cast<size_t>(element.Parent)].Children) {
      if (markup.Nodes()[static_cast<size_t>(sibling)].Kind != NodeKind::Element) { continue; }
      ++position;
      if (sibling == node) { break; }
    }
    if (position != compound.NthChild) { return false; }
  }
  if (!compound.Id.empty()) {
    const std::string *id = markup.AttributeOf(node, "id");
    if (id == nullptr || Lower(*id) != compound.Id) { return false; }
  }
  if (!compound.Classes.empty()) {
    const std::string *classes = markup.AttributeOf(node, "class");
    if (classes == nullptr) { return false; }
    const std::string lowered = Lower(*classes);
    for (const std::string &wanted : compound.Classes) {
      bool found = false;
      size_t at = 0;
      while (at < lowered.size()) {
        while (at < lowered.size() && Space(lowered[at])) { ++at; }
        const size_t from = at;
        while (at < lowered.size() && !Space(lowered[at])) { ++at; }
        if (at > from && lowered.compare(from, at - from, wanted) == 0) { found = true; }
      }
      if (!found) { return false; }
    }
  }
  return true;
}

} // namespace

namespace {

struct Boundary {
  const char *Name;
  const char *Why;
};

const std::array<Boundary, 136> kBoundaries = {{

    {.Name = "float",
     .Why = "an interface is laid out by flow and flexbox; a float solves a document's problem"},
    {.Name = "clear", .Why = "there is nothing to clear where nothing floats"},
    {.Name = "display:grid",
     .Why = "grid is a second two-dimensional model beside flexbox, and one is enough"},
    {.Name = "display:inline-grid", .Why = "the same, inline"},
    {.Name = "grid", .Why = "every grid property belongs to a model this engine does not carry"},
    {.Name = "display:table",
     .Why = "a table is a third layout model, and an interface states its rows itself"},
    {.Name = "display:inline-table", .Why = "the same, inline"},
    {.Name = "display:table-cell", .Why = "the same, one box down"},
    {.Name = "display:table-row", .Why = "the same, one box down"},
    {.Name = "display:list-item",
     .Why = "a marker box is inline layout with a second box beside the content"},
    {.Name = "<table>",
     .Why = "a table is a third layout model, and an interface states its rows itself"},
    {.Name = "<thead>", .Why = "the same"},
    {.Name = "<tbody>", .Why = "the same"},
    {.Name = "<tfoot>", .Why = "the same"},
    {.Name = "<tr>", .Why = "the same"},
    {.Name = "<td>", .Why = "the same"},
    {.Name = "<th>", .Why = "the same"},
    {.Name = "<caption>", .Why = "the same"},
    {.Name = "<colgroup>", .Why = "the same"},
    {.Name = "<col>", .Why = "the same"},

    {.Name = "display:inline-block", .Why = "inline layout beyond a text run is a second engine"},
    {.Name = "vertical-align",
     .Why = "it aligns things inside a line box, and there is one run per box here"},
    {.Name = "line-height:normal",
     .Why = "a normal line height is the font's own, and a face here states a number"},

    {.Name = "writing-mode",
     .Why = "one writing direction, declared; the rest is a second axis question everywhere"},
    {.Name = "direction", .Why = "the same, for the inline direction"},
    {.Name = "unicode-bidi", .Why = "the same, inside a run"},
    {.Name = "text-orientation", .Why = "the same, per glyph"},
    {.Name = "padding-inline",
     .Why = "a logical property is the writing direction under another name"},
    {.Name = "padding-block", .Why = "the same"},
    {.Name = "margin-inline", .Why = "the same"},
    {.Name = "margin-block", .Why = "the same"},
    {.Name = "border-inline", .Why = "the same"},
    {.Name = "border-block", .Why = "the same"},
    {.Name = "inset-inline", .Why = "the same"},
    {.Name = "inset-block", .Why = "the same"},
    {.Name = "inline-size", .Why = "the same, for a size"},
    {.Name = "block-size", .Why = "the same, for a size"},
    {.Name = "min-inline-size", .Why = "the same"},
    {.Name = "max-inline-size", .Why = "the same"},
    {.Name = "min-block-size", .Why = "the same"},
    {.Name = "max-block-size", .Why = "the same"},

    {.Name = "transform", .Why = "a matrix per box is a pass the frame budget has not bought"},
    {.Name = "box-shadow", .Why = "a shadow is a blur pass, and a declaration may not buy one"},
    {.Name = "text-shadow", .Why = "the same, per glyph"},
    {.Name = "filter",
     .Why = "a filter is a pass, declared by content, which is the door glTF closed"},
    {.Name = "backdrop-filter", .Why = "the same, over what is behind"},
    {.Name = "mix-blend-mode",
     .Why = "a blend mode per box is a pipeline state content may not switch"},
    {.Name = "background-image",
     .Why = "an image behind a box is a texture the consumer names, not a URL"},
    {.Name = "background:url", .Why = "the same, in the shorthand"},
    {.Name = "clip-path", .Why = "a shape per box is a second clip model beside the rectangle"},

    {.Name = "overflow:scroll",
     .Why = "scrolling is a client's state, and the library clips and reports"},
    {.Name = "overflow:auto", .Why = "the same, decided by the client"},
    {.Name = "overflow-x", .Why = "one overflow, both axes; a per-axis clip is a scroll model"},
    {.Name = "overflow-y", .Why = "the same"},
    {.Name = ":hover",
     .Why = "a pointer's state is the client's; the library answers what was hit"},
    {.Name = ":focus", .Why = "the same, for focus"},
    {.Name = ":active", .Why = "the same, while held"},
    {.Name = "cursor", .Why = "which cursor to show is the client's decision about meaning"},
    {.Name = "pointer-events", .Why = "the same, and the library already reports what a point hit"},

    {.Name = "calc",
     .Why = "arithmetic in a value is a second grammar; a consumer computes and declares"},
    {.Name = "var", .Why = "a custom property is a second cascade"},
    {.Name = "@media",
     .Why = "a conditional stylesheet is the consumer choosing which sheet to hand over"},
    {.Name = "@supports", .Why = "the same, on a capability the consumer already knows"},
    {.Name = "@font-face",
     .Why = "a face is an asset the consumer supplies through the font interface"},
    {.Name = "@import", .Why = "a sheet the consumer chooses to hand over"},
    {.Name = "@keyframes", .Why = "animation is the consumer re-declaring, frame by frame"},
    {.Name = "animation", .Why = "the same"},
    {.Name = "transition", .Why = "the same"},
    {.Name = "::before", .Why = "a box with no element is content in a stylesheet"},
    {.Name = "::after", .Why = "the same"},
    {.Name = "::first-letter", .Why = "the same, inside a run"},
    {.Name = "::first-line", .Why = "the same, per line"},
    {.Name = "::marker", .Why = "the same, beside a list item"},

    {.Name = "<img>", .Why = "a replaced box takes its size from a resource the consumer owns"},
    {.Name = "<picture>", .Why = "the same"},
    {.Name = "<source>", .Why = "the same"},
    {.Name = "<video>", .Why = "the same"},
    {.Name = "<audio>", .Why = "the same"},
    {.Name = "<canvas>", .Why = "the same"},
    {.Name = "<iframe>", .Why = "the same"},
    {.Name = "<embed>", .Why = "the same"},
    {.Name = "<object>", .Why = "the same"},
    {.Name = "<svg>", .Why = "the same"},
    {.Name = "<input>", .Why = "an interactive replaced box is the consumer's control"},
    {.Name = "<select>", .Why = "the same"},
    {.Name = "<textarea>", .Why = "the same"},
    {.Name = "<button>", .Why = "the same"},
    {.Name = "<label>", .Why = "the same"},
    {.Name = "<fieldset>", .Why = "the same"},
    {.Name = "<legend>", .Why = "the same"},
    {.Name = "<progress>", .Why = "the same"},
    {.Name = "<meter>", .Why = "the same"},
    {.Name = "<details>", .Why = "the same"},
    {.Name = "<summary>", .Why = "the same"},
    {.Name = "<form>", .Why = "the same"},
    {.Name = "<marquee>", .Why = "the same"},
    {.Name = "<math>", .Why = "a second content model with its own layout"},
    {.Name = "aspect-ratio", .Why = "a ratio sizes a replaced box, and this engine places none"},
    {.Name = "object-fit", .Why = "the same"},

    {.Name = "position:absolute",
     .Why = "deferred, and it carries its own work item -- not refused"},
    {.Name = "position:fixed", .Why = "the same"},
    {.Name = "position:sticky", .Why = "the same"},
    {.Name = "top", .Why = "an offset means nothing until a positioned box does"},
    {.Name = "bottom", .Why = "the same"},
    {.Name = "left", .Why = "the same"},
    {.Name = "right", .Why = "the same"},
    {.Name = "z-index",
     .Why = "a stacking order needs a stacking context, which absolute positioning brings"},

    {.Name = "font:",
     .Why = "the shorthand carries a family list, a style and a variant; a face here is one face"},
    {.Name = "font-family", .Why = "one family at a time, supplied by the consumer"},
    {.Name = "font-weight", .Why = "a weight is a second face, and a consumer supplies faces"},
    {.Name = "font-style", .Why = "the same"},
    {.Name = "font-variant", .Why = "the same"},
    {.Name = "font-stretch", .Why = "the same"},

    {.Name = "contain",
     .Why = "containment is a hint about a layout boundary this engine already knows exactly"},
    {.Name = "content-visibility", .Why = "the same, deciding whether to lay out at all"},
    {.Name = "will-change", .Why = "a hint to a compositor this engine does not have"},
    {.Name = "outline",
     .Why = "a second frame outside the box, and a border is the one an interface declares"},
    {.Name = "scrollbar",
     .Why = "a scrollbar is a control the client draws, from the overflow the library reports"},
    {.Name = "appearance",
     .Why = "a native widget look is a platform's, and this engine has no platform"},
    {.Name = "zoom", .Why = "a scale factor per box is a transform under another name"},
    {.Name = "visibility",
     .Why = "a box that takes room and draws nothing is a second kind of hidden; opacity says it"},
    {.Name = "resize", .Why = "a control the client owns"},
    {.Name = "user-select", .Why = "text selection is client state, like focus and hover"},
    {.Name = "quotes", .Why = "generated content in a stylesheet"},
    {.Name = "counter", .Why = "the same, counted"},
    {.Name = "list-style",
     .Why = "a marker box is inline layout with a second box beside the content"},

    {.Name = "border-spacing", .Why = "a table's own spacing, and there are no tables"},
    {.Name = "inset", .Why = "an offset means nothing until a positioned box does"},
    {.Name = "background:currentcolor",
     .Why = "a value that resolves against another property is a second cascade pass"},
    {.Name = "color:currentcolor", .Why = "the same"},
    {.Name = "nested-rule",
     .Why = "a nested rule is the same cascade written shorter, and a consumer can write it out"},
    {.Name = "!important", .Why = "a declaration that outranks the cascade is a second cascade"},
    {.Name = "+",
     .Why = "the adjacent-sibling combinator walks the tree sideways, which no declaration here "
            "asks for"},
    {.Name = "~", .Why = "the same, further along"},
    {.Name = "the document closes",
     .Why = "this reader refuses a stray end tag that HTML ignores, so a consumer's declaration is "
            "right "
            "or "
            "it is refused -- upstream's corpus is not that consumer"},

    {.Name = "a script in the document decides this layout",
     .Why = "this engine runs a declared handler and never a document's own program"},
}};

} // namespace

const char *WhyOutside(std::string_view name) {
  for (const Boundary &boundary : kBoundaries) {
    const std::string_view row(boundary.Name);
    if (name.size() < row.size()) { continue; }

    const bool fragment =
        row.front() == ':' || row.front() == '+' || row.front() == '~' || row.front() == '!';
    const bool matched = fragment ? name.contains(row) : name.starts_with(row);
    if (matched) { return boundary.Why; }
  }
  return nullptr;
}

namespace {

bool ChainSelects(const Rule &rule, const Markup &markup, size_t wanted, int node) {
  if (!Holds(rule.Chain[wanted], markup, node)) { return false; }
  if (wanted == 0) { return true; }
  const Reach reach = wanted - 1 < rule.Links.size() ? rule.Links[wanted - 1] : Reach::Descendant;
  const int parent = markup.Nodes()[static_cast<size_t>(node)].Parent;
  if (reach == Reach::Child) {
    return parent >= 0 && ChainSelects(rule, markup, wanted - 1, parent);
  }
  for (int at = parent; at >= 0; at = markup.Nodes()[static_cast<size_t>(at)].Parent) {
    if (ChainSelects(rule, markup, wanted - 1, at)) { return true; }
  }
  return false;
}
} // namespace

bool Selects(const Rule &rule, const Markup &markup, int node) {
  return !rule.Chain.empty() && ChainSelects(rule, markup, rule.Chain.size() - 1, node);
}

} // namespace outshine::Ui
