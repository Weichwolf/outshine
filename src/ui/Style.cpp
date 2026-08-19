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

/* THE TABLE IS THE SUBSET, and `board:1442` is where it is argued. A name absent here is a property
 * this engine does not hold, which is a fact the corpus selection reads rather than an oversight. */
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

/* THE NAMED COLOURS THE CORPUS USES, and it is a short list on purpose: a full CSS colour table is
 * 148 names of which this corpus writes a dozen, and every one it writes is here. A name outside it
 * is a property value this engine does not hold, counted like any other. */
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
    {"maroon", 0x800000FF},      {"olive", 0x808000FF},
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

/* A VALUE, READ ONCE. A number with a unit is a length; a bare number is a number; a colour is a
 * colour; anything else is a keyword and its spelling is hashed so a comparison is an integer. */
Value ReadValue(std::string_view text) {
  const std::string_view trimmed = Trim(text);
  Value value;
  if (trimmed.empty()) { return value; }
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
    char *stopped = nullptr;
    const std::string held(trimmed);
    const double number = std::strtod(held.c_str(), &stopped);
    const std::string_view suffix = Trim(std::string_view(stopped));
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
      /* A unit this engine does not hold -- `vh`, `ch`, `pt` -- and the value is not a length it can
       * resolve. It is left as `None` with the number kept, and the reader above counts it. */
      value.How = Unit::Keyword;
      value.Word = Keyword(trimmed);
    }
    return value;
  }
  value.How = Unit::Keyword;
  value.Word = Keyword(trimmed);
  return value;
}

}  // namespace

Property PropertyNamed(std::string_view name) {
  const std::string lowered = Lower(name);
  for (const Named &one : kProperties) {
    if (lowered == one.Spelling) { return one.What; }
  }
  return Property::kCount;
}

/* THE SHORTHANDS THE CORPUS WRITES, expanded here rather than carried as their own properties: a
 * shorthand is a spelling of several declarations and holding it as a value would make every reader
 * downstream ask which one it meant. */
namespace {

void Expand(std::string_view name, std::string_view text, std::vector<Declaration> &into,
            size_t &unheld, std::vector<std::string> &names) {
  const std::string lowered = Lower(name);
  const auto one = [&](Property what, std::string_view value) {
    into.push_back({what, ReadValue(value)});
  };
  /* `margin`, `padding` and `border-width` take one to four values in CSS's own clock order. */
  const auto sides = [&](Property top, Property right, Property bottom, Property left) {
    std::vector<std::string_view> parts;
    size_t at = 0;
    while (at < text.size()) {
      while (at < text.size() && Space(text[at])) { ++at; }
      const size_t from = at;
      while (at < text.size() && !Space(text[at])) { ++at; }
      if (at > from) { parts.push_back(text.substr(from, at - from)); }
    }
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
  } else {
    const Property what = PropertyNamed(lowered);
    if (what != Property::kCount) {
      into.push_back({what, ReadValue(text)});
      return;
    }
  }
  ++unheld;
  if (names.size() < 64) { names.push_back(lowered); }
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
  while (at < text.size()) {
    const char lead = text[at];
    if (lead == '.' || lead == '#') { ++at; }
    const size_t from = at;
    while (at < text.size() && text[at] != '.' && text[at] != '#') { ++at; }
    const std::string part = Lower(text.substr(from, at - from));
    if (part.empty()) { return false; }
    if (lead == '.') {
      out.Classes.push_back(part);
    } else if (lead == '#') {
      if (!out.Id.empty()) { return false; }
      out.Id = part;
    } else {
      if (!out.Tag.empty()) { return false; }
      /* `*`, `:hover`, `[attr]` and every combinator are outside the subset and refuse here. */
      for (const char c : part) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
          return false;
        }
      }
      out.Tag = part;
    }
  }
  return !(out.Tag.empty() && out.Classes.empty() && out.Id.empty());
}

}  // namespace

std::vector<Declaration> Stylesheet::Inline(std::string_view text) const {
  std::vector<Declaration> out;
  size_t unheld = 0;
  std::vector<std::string> names;
  ReadBlock(text, out, unheld, names);
  return out;
}

void Stylesheet::Read(std::string_view css) {
  size_t at = 0;
  while (at < css.size()) {
    /* A comment reaches no rule. */
    if (css.compare(at, 2, "/*") == 0) {
      const size_t end = css.find("*/", at + 2);
      at = end == std::string_view::npos ? css.size() : end + 2;
      continue;
    }
    if (Space(css[at])) {
      ++at;
      continue;
    }
    /* AN AT-RULE IS OUTSIDE THE SUBSET AND SAYS SO. `@media`, `@font-face` and `@import` each carry a
     * question this engine has no answer for, and skipping one silently would make a sheet mean
     * something it does not. */
    if (css[at] == '@') {
      ++Unselectable_;
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
    const size_t close = css.find('}', brace);
    const std::string_view heads = css.substr(at, brace - at);
    const std::string_view body =
        css.substr(brace + 1, (close == std::string_view::npos ? css.size() : close) - brace - 1);
    at = close == std::string_view::npos ? css.size() : close + 1;

    std::vector<Declaration> declares;
    ReadBlock(body, declares, Unheld_, Names_);

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
      size_t cursor = 0;
      while (cursor < head.size() && held) {
        while (cursor < head.size() && Space(head[cursor])) { ++cursor; }
        const size_t start = cursor;
        while (cursor < head.size() && !Space(head[cursor])) { ++cursor; }
        if (cursor == start) { break; }
        Compound compound;
        held = ReadCompound(head.substr(start, cursor - start), compound);
        if (held) { rule.Chain.push_back(std::move(compound)); }
      }
      if (!held || rule.Chain.empty()) {
        ++Unselectable_;
        continue;
      }
      for (const Compound &one : rule.Chain) {
        rule.Specificity += one.Id.empty() ? 0 : 10000;
        rule.Specificity += (int)one.Classes.size() * 100;
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

}  // namespace

bool Selects(const Rule &rule, const Markup &markup, int node) {
  if (rule.Chain.empty()) { return false; }
  /* THE SUBJECT IS THE LAST COMPOUND and the rest must be found among the ancestors, in order. That
   * is the descendant combinator and the only one here. */
  if (!Holds(rule.Chain.back(), markup, node)) { return false; }
  int at = markup.Nodes()[(size_t)node].Parent;
  size_t wanted = rule.Chain.size() - 1;
  while (wanted > 0 && at >= 0) {
    if (Holds(rule.Chain[wanted - 1], markup, at)) { --wanted; }
    at = markup.Nodes()[(size_t)at].Parent;
  }
  return wanted == 0;
}

}  // namespace outshine::Ui
