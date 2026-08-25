#include <charconv>
#include "Style.h"

#include <cstdlib>

namespace outshine::Ui {

namespace {

bool Space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }

std::string Lower(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) { out.push_back(c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c); }
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

const Named kProperties[] = {
    {"display", Property::Display},
    {"position", Property::Position},
    {"box-sizing", Property::BoxSizing},
    {"overflow", Property::Overflow},
    {"flex-direction", Property::FlexDirection},
    {"flex-wrap", Property::FlexWrap},
    {"justify-content", Property::JustifyContent},
    {"align-items", Property::AlignItems},
    {"align-self", Property::AlignSelf},
    {"align-content", Property::AlignContent},
    {"flex-grow", Property::FlexGrow},
    {"flex-shrink", Property::FlexShrink},
    {"flex-basis", Property::FlexBasis},
    {"gap", Property::Gap},
    {"row-gap", Property::RowGap},
    {"column-gap", Property::ColumnGap},
    {"width", Property::Width},
    {"height", Property::Height},
    {"min-width", Property::MinWidth},
    {"max-width", Property::MaxWidth},
    {"min-height", Property::MinHeight},
    {"max-height", Property::MaxHeight},
    {"margin-top", Property::MarginTop},
    {"margin-right", Property::MarginRight},
    {"margin-bottom", Property::MarginBottom},
    {"margin-left", Property::MarginLeft},
    {"padding-top", Property::PaddingTop},
    {"padding-right", Property::PaddingRight},
    {"padding-bottom", Property::PaddingBottom},
    {"padding-left", Property::PaddingLeft},
    {"border-top-width", Property::BorderTopWidth},
    {"border-right-width", Property::BorderRightWidth},
    {"border-bottom-width", Property::BorderBottomWidth},
    {"border-left-width", Property::BorderLeftWidth},
    {"top", Property::Top},
    {"right", Property::Right},
    {"bottom", Property::Bottom},
    {"left", Property::Left},
    {"background-color", Property::BackgroundColour},
    {"border-color", Property::BorderColour},
    {"border-radius", Property::BorderRadius},
    {"opacity", Property::Opacity},
    {"color", Property::Colour},
    {"font-size", Property::FontSize},
    {"line-height", Property::LineHeight},
    {"text-align", Property::TextAlign},
    {"white-space", Property::WhiteSpace},
    {"font-family", Property::FontFamily},
};

struct NamedColour {
  const char *Spelling;
  uint32_t Rgba;
};
const NamedColour kColours[] = {
    {"transparent", 0x00000000}, {"black", 0x000000FF},     {"white", 0xFFFFFFFF},
    {"red", 0xFF0000FF},         {"green", 0x008000FF},     {"blue", 0x0000FFFF},
    {"yellow", 0xFFFF00FF},      {"orange", 0xFFA500FF},    {"purple", 0x800080FF},
    {"gray", 0x808080FF},        {"grey", 0x808080FF},      {"silver", 0xC0C0C0FF},
    {"lightgray", 0xD3D3D3FF},   {"lightgrey", 0xD3D3D3FF}, {"lightgreen", 0x90EE90FF},
    {"pink", 0xFFC0CBFF},        {"teal", 0x008080FF},      {"navy", 0x000080FF},
    {"lime", 0x00FF00FF},        {"aqua", 0x00FFFFFF},      {"fuchsia", 0xFF00FFFF},
    {"maroon", 0x800000FF},      {"olive", 0x808000FF},      {"lightblue", 0xADD8E6FF},
    {"salmon", 0xFA8072FF},      {"cyan", 0x00FFFFFF},       {"magenta", 0xFF00FFFF},
    {"brown", 0xA52A2AFF},       {"gold", 0xFFD700FF},       {"violet", 0xEE82EEFF},
    {"indigo", 0x4B0082FF},      {"beige", 0xF5F5DCFF},      {"tan", 0xD2B48CFF},
    {"coral", 0xFF7F50FF},       {"khaki", 0xF0E68CFF},      {"plum", 0xDDA0DDFF},
    {"orchid", 0xDA70D6FF},      {"skyblue", 0x87CEEBFF},    {"steelblue", 0x4682B4FF},
    {"darkgray", 0xA9A9A9FF},    {"darkgrey", 0xA9A9A9FF},   {"lightyellow", 0xFFFFE0FF},
    {"lightpink", 0xFFB6C1FF},   {"lightcyan", 0xE0FFFFFF},  {"seagreen", 0x2E8B57FF},
    {"darkblue", 0x00008BFF},    {"darkgreen", 0x006400FF},  {"darkred", 0x8B0000FF},
    {"hotpink", 0xFF69B4FF},     {"papayawhip", 0xFFEFD5FF}, {"whitesmoke", 0xF5F5F5FF},
    {"gainsboro", 0xDCDCDCFF},   {"peachpuff", 0xFFDAB9FF},  {"lavender", 0xE6E6FAFF},
    {"turquoise", 0x40E0D0FF},   {"crimson", 0xDC143CFF},    {"chocolate", 0xD2691EFF},
    {"goldenrod", 0xDAA520FF},   {"firebrick", 0xB22222FF},  {"forestgreen", 0x228B22FF},
    {"midnightblue", 0x191970FF},{"royalblue", 0x4169E1FF},  {"slategray", 0x708090FF},
    {"slategrey", 0x708090FF},   {"dimgray", 0x696969FF},    {"dimgrey", 0x696969FF},
};

int HexOf(char c) {
  if (c >= '0' && c <= '9') { return c - '0'; }
  if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
  if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
  return -1;
}

bool ReadColour(std::string_view text, uint32_t &out) {
  if (!text.empty() && text[0] == '#') {
    const std::string_view digits = text.substr(1);
    int channel[4] = {0, 0, 0, 255};
    if (digits.size() == 3 || digits.size() == 4) {
      for (size_t at = 0; at < digits.size(); ++at) {
        const int one = HexOf(digits[at]);
        if (one < 0) { return false; }
        channel[at] = one * 17;
      }
    } else if (digits.size() == 6 || digits.size() == 8) {
      for (size_t at = 0; at + 1 < digits.size(); at += 2) {
        const int high = HexOf(digits[at]), low = HexOf(digits[at + 1]);
        if (high < 0 || low < 0) { return false; }
        channel[at / 2] = high * 16 + low;
      }
    } else {
      return false;
    }
    out = ((uint32_t)channel[0] << 24) | ((uint32_t)channel[1] << 16) | ((uint32_t)channel[2] << 8) |
          (uint32_t)channel[3];
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
        Trim(digits.substr((size_t)(scanned.ptr - digits.data())));
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

}

Property PropertyNamed(std::string_view name) {
  const std::string lowered = Lower(name);
  for (const Named &one : kProperties) {
    if (lowered == one.Spelling) { return one.What; }
  }
  return Property::kCount;
}

struct Vocabulary {
  Property What;
  const char *Words[14];
};
const Vocabulary kVocabularies[] = {
    {Property::Display, {"block", "flex", "inline", "none", "inline-flex", nullptr}},
    {Property::Position, {"static", "relative", nullptr}},
    {Property::BoxSizing, {"content-box", "border-box", nullptr}},
    {Property::Overflow, {"visible", "hidden", "auto", "scroll", nullptr}},
    {Property::FlexDirection, {"row", "column", "row-reverse", "column-reverse", nullptr}},
    {Property::FlexWrap, {"nowrap", "wrap", "wrap-reverse", nullptr}},
    {Property::JustifyContent,
     {"flex-start", "flex-end", "center", "space-between", "space-around", "space-evenly", "start",
      "end", "left", "right", "normal", "stretch", nullptr}},
    {Property::AlignItems,
     {"stretch", "flex-start", "flex-end", "center", "start", "end", "baseline", "normal", nullptr}},
    {Property::AlignSelf,
     {"auto", "stretch", "flex-start", "flex-end", "center", "start", "end", "baseline", "normal",
      nullptr}},
    {Property::AlignContent,
     {"stretch", "flex-start", "flex-end", "center", "space-between", "space-around", "space-evenly",
      "start", "end", "normal", nullptr}},
    {Property::TextAlign, {"left", "right", "center", nullptr}},
    {Property::WhiteSpace, {"normal", "pre", nullptr}},
};

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

namespace {

bool Bare(std::string_view text) {
  const Value value = ReadValue(text);
  return value.How == Unit::None;
}

void Expand(std::string_view name, std::string_view text, std::vector<Declaration> &into,
            size_t &unheld, std::vector<std::string> &names) {
  const std::string lowered = Lower(name);

  if (!lowered.empty() && lowered.front() == '-') { return; }
  const auto one = [&](Property what, std::string_view value) {
    into.push_back({what, ReadValue(value)});
  };
  const auto words = [&]() {
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

  const auto flex = [&]() {
    const std::vector<std::string_view> parts = words();
    if (parts.empty() || parts.size() > 3) { return false; }
    if (parts.size() == 1 && (parts[0] == "none" || parts[0] == "auto" || parts[0] == "initial")) {
      one(Property::FlexGrow, parts[0] == "auto" ? "1" : "0");
      one(Property::FlexShrink, parts[0] == "none" ? "0" : "1");
      one(Property::FlexBasis, "auto");
      return true;
    }
    size_t numbers = 0, basisAt = parts.size();
    std::string_view bare[2];
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
      size_t first = parts.size(), second = parts.size();
      for (size_t at = 0; at < parts.size(); ++at) {
        if (!Bare(parts[at])) { continue; }
        if (first == parts.size()) { first = at; } else { second = at; }
      }
      if (basisAt > first && basisAt < second) { return true; }
    }
    one(Property::FlexGrow, bare[0]);
    one(Property::FlexShrink, numbers > 1 ? bare[1] : std::string_view("1"));

    one(Property::FlexBasis, basisAt < parts.size() ? parts[basisAt] : std::string_view("0%"));
    return true;
  };
  const auto flexFlow = [&]() {
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
    bool sawStyle = false, drawn = true;
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
    if (sides(Property::MarginTop, Property::MarginRight, Property::MarginBottom,
              Property::MarginLeft)) {
      return;
    }
  } else if (lowered == "padding") {
    if (sides(Property::PaddingTop, Property::PaddingRight, Property::PaddingBottom,
              Property::PaddingLeft)) {
      return;
    }
  } else if (lowered == "border-width") {
    if (sides(Property::BorderTopWidth, Property::BorderRightWidth, Property::BorderBottomWidth,
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
    if (border(Property::BorderTopWidth, Property::BorderRightWidth, Property::BorderBottomWidth,
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
      into.push_back({what, value});
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

void ReadBlock(std::string_view body, std::vector<Declaration> &into, size_t &unheld,
               std::vector<std::string> &names) {
  size_t at = 0;
  while (at < body.size()) {
    const size_t semi = body.find(';', at);
    const std::string_view one = Trim(body.substr(at, semi == std::string_view::npos
                                                         ? std::string_view::npos
                                                         : semi - at));
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
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
        return false;
      }
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
  return out.Universal ||
         !(out.Tag.empty() && out.Classes.empty() && out.Id.empty() && out.NthChild == 0);
}

}

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
        Names_.emplace_back(css.substr(at, (stops == std::string_view::npos ? css.size() : stops) - at));
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
              declares, Unheld_, Names_);

    size_t from = 0;
    while (from <= heads.size()) {
      const size_t comma = heads.find(',', from);
      const std::string_view head =
          Trim(heads.substr(from, comma == std::string_view::npos ? std::string_view::npos
                                                                 : comma - from));
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
        rule.Specificity += one.Id.empty() ? 0 : 10000;

        rule.Specificity += (int)(one.Classes.size() + (one.NthChild > 0 ? 1u : 0u)) * 100;
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
  const Node &element = markup.Nodes()[(size_t)node];
  if (element.Kind != NodeKind::Element) { return false; }
  if (!compound.Tag.empty() && element.Name != compound.Tag) { return false; }
  if (compound.NthChild > 0) {

    if (element.Parent < 0) { return false; }
    int position = 0;
    for (const int sibling : markup.Nodes()[(size_t)element.Parent].Children) {
      if (markup.Nodes()[(size_t)sibling].Kind != NodeKind::Element) { continue; }
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

}

namespace {

struct Boundary {
  const char *Name;
  const char *Why;
};

const Boundary kBoundaries[] = {

    {"float", "an interface is laid out by flow and flexbox; a float solves a document's problem"},
    {"clear", "there is nothing to clear where nothing floats"},
    {"display:grid", "grid is a second two-dimensional model beside flexbox, and one is enough"},
    {"display:inline-grid", "the same, inline"},
    {"grid", "every grid property belongs to a model this engine does not carry"},
    {"display:table", "a table is a third layout model, and an interface states its rows itself"},
    {"display:inline-table", "the same, inline"},
    {"display:table-cell", "the same, one box down"},
    {"display:table-row", "the same, one box down"},
    {"display:list-item", "a marker box is inline layout with a second box beside the content"},
    {"<table>", "a table is a third layout model, and an interface states its rows itself"},
    {"<thead>", "the same"},   {"<tbody>", "the same"},   {"<tfoot>", "the same"},
    {"<tr>", "the same"},      {"<td>", "the same"},      {"<th>", "the same"},
    {"<caption>", "the same"}, {"<colgroup>", "the same"}, {"<col>", "the same"},

    {"display:inline-block", "inline layout beyond a text run is a second engine"},
    {"vertical-align", "it aligns things inside a line box, and there is one run per box here"},
    {"line-height:normal", "a normal line height is the font's own, and a face here states a number"},

    {"writing-mode", "one writing direction, declared; the rest is a second axis question everywhere"},
    {"direction", "the same, for the inline direction"},
    {"unicode-bidi", "the same, inside a run"},
    {"text-orientation", "the same, per glyph"},
    {"padding-inline", "a logical property is the writing direction under another name"},
    {"padding-block", "the same"},
    {"margin-inline", "the same"},
    {"margin-block", "the same"},
    {"border-inline", "the same"},
    {"border-block", "the same"},
    {"inset-inline", "the same"},
    {"inset-block", "the same"},
    {"inline-size", "the same, for a size"},
    {"block-size", "the same, for a size"},
    {"min-inline-size", "the same"}, {"max-inline-size", "the same"},
    {"min-block-size", "the same"},  {"max-block-size", "the same"},

    {"transform", "a matrix per box is a pass the frame budget has not bought"},
    {"box-shadow", "a shadow is a blur pass, and a declaration may not buy one"},
    {"text-shadow", "the same, per glyph"},
    {"filter", "a filter is a pass, declared by content, which is the door glTF closed"},
    {"backdrop-filter", "the same, over what is behind"},
    {"mix-blend-mode", "a blend mode per box is a pipeline state content may not switch"},
    {"background-image", "an image behind a box is a texture the consumer names, not a URL"},
    {"background:url", "the same, in the shorthand"},
    {"clip-path", "a shape per box is a second clip model beside the rectangle"},

    {"overflow:scroll", "scrolling is a client's state, and the library clips and reports"},
    {"overflow:auto", "the same, decided by the client"},
    {"overflow-x", "one overflow, both axes; a per-axis clip is a scroll model"},
    {"overflow-y", "the same"},
    {":hover", "a pointer's state is the client's; the library answers what was hit"},
    {":focus", "the same, for focus"},
    {":active", "the same, while held"},
    {"cursor", "which cursor to show is the client's decision about meaning"},
    {"pointer-events", "the same, and the library already reports what a point hit"},

    {"calc", "arithmetic in a value is a second grammar; a consumer computes and declares"},
    {"var", "a custom property is a second cascade"},
    {"@media", "a conditional stylesheet is the consumer choosing which sheet to hand over"},
    {"@supports", "the same, on a capability the consumer already knows"},
    {"@font-face", "a face is an asset the consumer supplies through the font interface"},
    {"@import", "a sheet the consumer chooses to hand over"},
    {"@keyframes", "animation is the consumer re-declaring, frame by frame"},
    {"animation", "the same"},
    {"transition", "the same"},
    {"::before", "a box with no element is content in a stylesheet"},
    {"::after", "the same"},
    {"::first-letter", "the same, inside a run"},
    {"::first-line", "the same, per line"},
    {"::marker", "the same, beside a list item"},

    {"<img>", "a replaced box takes its size from a resource the consumer owns"},
    {"<picture>", "the same"},  {"<source>", "the same"},  {"<video>", "the same"},
    {"<audio>", "the same"},    {"<canvas>", "the same"},  {"<iframe>", "the same"},
    {"<embed>", "the same"},    {"<object>", "the same"},  {"<svg>", "the same"},
    {"<input>", "an interactive replaced box is the consumer's control"},
    {"<select>", "the same"},   {"<textarea>", "the same"},{"<button>", "the same"},
    {"<label>", "the same"},    {"<fieldset>", "the same"},{"<legend>", "the same"},
    {"<progress>", "the same"}, {"<meter>", "the same"},   {"<details>", "the same"},
    {"<summary>", "the same"},  {"<form>", "the same"},    {"<marquee>", "the same"},
    {"<math>", "a second content model with its own layout"},
    {"aspect-ratio", "a ratio sizes a replaced box, and this engine places none"},
    {"object-fit", "the same"},

    {"position:absolute", "deferred, and it carries its own work item -- not refused"},
    {"position:fixed", "the same"},
    {"position:sticky", "the same"},
    {"top", "an offset means nothing until a positioned box does"},
    {"bottom", "the same"}, {"left", "the same"}, {"right", "the same"},
    {"z-index", "a stacking order needs a stacking context, which absolute positioning brings"},

    {"font:", "the shorthand carries a family list, a style and a variant; a face here is one face"},
    {"font-family", "one family at a time, supplied by the consumer"},
    {"font-weight", "a weight is a second face, and a consumer supplies faces"},
    {"font-style", "the same"},
    {"font-variant", "the same"},
    {"font-stretch", "the same"},

    {"contain", "containment is a hint about a layout boundary this engine already knows exactly"},
    {"content-visibility", "the same, deciding whether to lay out at all"},
    {"will-change", "a hint to a compositor this engine does not have"},
    {"outline", "a second frame outside the box, and a border is the one an interface declares"},
    {"scrollbar", "a scrollbar is a control the client draws, from the overflow the library reports"},
    {"appearance", "a native widget look is a platform's, and this engine has no platform"},
    {"zoom", "a scale factor per box is a transform under another name"},
    {"visibility", "a box that takes room and draws nothing is a second kind of hidden; opacity says it"},
    {"resize", "a control the client owns"},
    {"user-select", "text selection is client state, like focus and hover"},
    {"quotes", "generated content in a stylesheet"},
    {"counter", "the same, counted"},
    {"list-style", "a marker box is inline layout with a second box beside the content"},

    {"border-spacing", "a table's own spacing, and there are no tables"},
    {"inset", "an offset means nothing until a positioned box does"},
    {"background:currentcolor",
     "a value that resolves against another property is a second cascade pass"},
    {"color:currentcolor", "the same"},
    {"nested-rule", "a nested rule is the same cascade written shorter, and a consumer can write it out"},
    {"!important", "a declaration that outranks the cascade is a second cascade"},
    {"+", "the adjacent-sibling combinator walks the tree sideways, which no declaration here asks for"},
    {"~", "the same, further along"},
    {"the document closes",
     "this reader refuses a stray end tag that HTML ignores, so a consumer's declaration is right or "
     "it is refused -- upstream's corpus is not that consumer"},

    {"a script in the document decides this layout",
     "this engine runs a declared handler and never a document's own program"},
};

}

const char *WhyOutside(std::string_view name) {
  for (const Boundary &boundary : kBoundaries) {
    const std::string_view row(boundary.Name);
    if (name.size() < row.size()) { continue; }

    const bool fragment = row.front() == ':' || row.front() == '+' || row.front() == '~' ||
                          row.front() == '!';
    const bool matched = fragment ? name.find(row) != std::string_view::npos
                                  : name.compare(0, row.size(), row) == 0;
    if (matched) { return boundary.Why; }
  }
  return nullptr;
}

bool ChainSelects(const Rule &rule, const Markup &markup, size_t wanted, int node) {
  if (!Holds(rule.Chain[wanted], markup, node)) { return false; }
  if (wanted == 0) { return true; }
  const Reach reach = wanted - 1 < rule.Links.size() ? rule.Links[wanted - 1] : Reach::Descendant;
  const int parent = markup.Nodes()[(size_t)node].Parent;
  if (reach == Reach::Child) {
    return parent >= 0 && ChainSelects(rule, markup, wanted - 1, parent);
  }
  for (int at = parent; at >= 0; at = markup.Nodes()[(size_t)at].Parent) {
    if (ChainSelects(rule, markup, wanted - 1, at)) { return true; }
  }
  return false;
}

bool Selects(const Rule &rule, const Markup &markup, int node) {
  return !rule.Chain.empty() && ChainSelects(rule, markup, rule.Chain.size() - 1, node);
}

}
