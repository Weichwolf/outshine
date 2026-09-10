#include "Xml.h"
#include "XmlAttribute.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <string>
#include <limits>
#include <string_view>

namespace outshine {
namespace Says {
constexpr auto kXmlInputTooLarge = "XML input exceeds the 16 MiB text budget";
}

namespace {

constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";

constexpr size_t kXmlMaxTextBytes = size_t{16} * 1024u * 1024u;
static_assert(kXmlMaxTextBytes <= std::numeric_limits<uint32_t>::max());

bool Space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool NameStart(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool NameChar(char c) {
  return NameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

}

struct Xml::ParseState {
  std::array<uint32_t, kXmlMaxDepth> Stack{};
  std::array<uint32_t, kXmlMaxDepth> LastChild{};
  size_t Depth = 0;
  size_t At = 0;
  bool Closed = false;
  std::string DecodedAttribute;
};

bool Xml::Parse(const char *text, size_t length) {
  Error_.clear();
  SiblingSteps_ = 0;
  Nodes_.clear();
  Attributes_.clear();
  Asked_.clear();
  Root_ = 0;
  if (text == nullptr) { return Refuse("there is no document to read", 0); }

  if (length > kXmlMaxTextBytes) { return Refuse(Says::kXmlInputTooLarge, 0); }

  Text_.assign(text, length);
  Nodes_.emplace_back();

  ParseState state;
  if (Text_.starts_with(kUtf8Bom)) { state.At = kUtf8Bom.size(); }
  while (state.At < length) {
    if (!ParseMarkup(state)) { return false; }
  }
  if (state.Depth != 0) {
    const Node &open = Nodes_[state.Stack[state.Depth - 1]];
    return Refuse("the element '" + Span(open.NameOff, open.NameLen) + "' is never closed", length);
  }
  if (Root_ == 0) { return Refuse("the document carries no element", length); }
  Asked_.assign(Attributes_.size(), 0);
  return true;
}

bool Xml::ParseText(ParseState &state) {
  const size_t length = Text_.size();
  auto &at = state.At;
  const auto &depth = state.Depth;
  const auto &stack = state.Stack;
  const size_t from = at;
  while (at < length && Text_[at] != '<') { ++at; }
  size_t start = from;
  size_t stop = at;
  while (start < stop && Space(Text_[start])) { ++start; }
  while (stop > start && Space(Text_[stop - 1])) { --stop; }
  if (stop == start) { return true; }
  if (depth == 0) { return Refuse("only whitespace is allowed outside the root element", start); }
  Node &into = Nodes_[stack[depth - 1]];
  into.TextOff = static_cast<uint32_t>(start);
  into.TextLen = static_cast<uint32_t>(stop - start);
  return true;
}

bool Xml::ParseMarkup(ParseState &state) {
  const size_t length = Text_.size();
  auto &at = state.At;
  if (Text_[at] != '<') { return ParseText(state); }
  if (at + 1 < length && Text_[at + 1] == '?') {
    const size_t stop = Text_.find("?>", at);
    if (stop == std::string::npos) { return Refuse("a processing instruction never ends", at); }
    at = stop + 2;
    return true;
  }
  if (at + 3 < length && Text_.compare(at, 4, "<!--") == 0) {
    const size_t stop = Text_.find("-->", at);
    if (stop == std::string::npos) { return Refuse("a comment never ends", at); }
    if (Text_.find("--", at + 4) != stop) {
      return Refuse("a comment cannot contain a double hyphen", at);
    }
    at = stop + 3;
    return true;
  }
  if (at + 1 < length && Text_[at + 1] == '!') {
    return Refuse("this reader takes elements, attributes and text, and a declaration beginning "
                  "'<!' is a doctype or a section it does not",
                  at);
  }

  if (at + 1 < length && Text_[at + 1] == '/') { return ParseClosingTag(state); }
  return ParseOpeningTag(state);
}

bool Xml::ParseClosingTag(ParseState &state) {
  const size_t length = Text_.size();
  auto &at = state.At;
  auto &depth = state.Depth;
  const auto &stack = state.Stack;
  auto &closed = state.Closed;
  const size_t name = at + 2;
  size_t stop = name;
  while (stop < length && NameChar(Text_[stop])) { ++stop; }
  if (depth == 0) { return Refuse("a closing tag closes an element nothing opened", at); }
  const Node &open = Nodes_[stack[depth - 1]];
  if (open.NameLen != stop - name ||
      std::memcmp(Text_.data() + open.NameOff, Text_.data() + name, stop - name) != 0) {
    return Refuse("a closing tag names '" +
                      Span(static_cast<uint32_t>(name), static_cast<uint32_t>(stop - name)) +
                      "' and the open element is '" + Span(open.NameOff, open.NameLen) + "'",
                  at);
  }
  while (stop < length && Space(Text_[stop])) { ++stop; }
  if (stop >= length || Text_[stop] != '>') {
    return Refuse("a closing tag allows only whitespace after its name, then '>'", stop);
  }
  --depth;
  if (depth == 0) { closed = true; }
  at = stop + 1;
  return true;
}

bool Xml::ParseOpeningTag(ParseState &state) {
  const size_t length = Text_.size();
  auto &at = state.At;
  auto &depth = state.Depth;
  auto &stack = state.Stack;
  auto &closed = state.Closed;
  const size_t name = at + 1;
  if (name >= length || !NameStart(Text_[name])) {
    return Refuse("an element's name begins with a letter or an underscore", at);
  }
  size_t stop = name;
  while (stop < length && NameChar(Text_[stop])) { ++stop; }
  if (stop < length && Text_[stop] == ':') {
    return Refuse("this reader declares no namespaces, and '" +
                      Span(static_cast<uint32_t>(name), static_cast<uint32_t>(stop - name)) +
                      ":' is one",
                  at);
  }
  if (closed) { return Refuse("a document carries one root element and this is a second", at); }
  if (Nodes_.size() >= kXmlMaxNodes) {
    return Refuse("the document reaches the element bound of " + std::to_string(kXmlMaxNodes), at);
  }

  Nodes_.emplace_back();
  const auto made = static_cast<uint32_t>(Nodes_.size() - 1);
  Nodes_[made].NameOff = static_cast<uint32_t>(name);
  Nodes_[made].NameLen = static_cast<uint32_t>(stop - name);
  Nodes_[made].FirstAttribute = static_cast<uint32_t>(Attributes_.size());

  if (depth == 0) {
    Root_ = made;
  } else {
    const uint32_t parent = stack[depth - 1];
    const uint32_t last = state.LastChild[depth - 1];
    if (last == 0) {
      Nodes_[parent].FirstChild = made;
    } else {
      Nodes_[last].NextSibling = made;
    }
    state.LastChild[depth - 1] = made;
  }
  at = stop;
  bool empty = false;
  if (!ParseAttributes(state, made, empty)) { return false; }
  if (!empty) {
    if (depth >= kXmlMaxDepth) {
      return Refuse("the document nests past the depth bound of " + std::to_string(kXmlMaxDepth),
                    name);
    }
    state.LastChild[depth] = 0;
    stack[depth++] = made;
  } else if (depth == 0) {
    closed = true;
  }
  return true;
}

bool Xml::ParseAttributes(ParseState &state, uint32_t made, bool &empty) {
  const size_t length = Text_.size();
  auto &at = state.At;
  while (at < length) {
    const size_t beforeSpace = at;
    while (at < length && Space(Text_[at])) { ++at; }
    if (at < length && Text_[at] == '/') {
      if (at + 1 >= length || Text_[at + 1] != '>') {
        return Refuse("a self-closing tag ends with '/>'", at);
      }
      empty = true;
      at += 2;
      return true;
    }
    if (at < length && Text_[at] == '>') {
      ++at;
      return true;
    }
    if (at >= length) { return Refuse("an element's tag never ends", Nodes_[made].NameOff); }
    if (at == beforeSpace) { return Refuse("an attribute must be preceded by whitespace", at); }
    if (!ParseAttribute(state, made)) { return false; }
  }
  return Refuse("an element's tag never ends", Nodes_[made].NameOff);
}

bool Xml::ParseAttribute(ParseState &state, uint32_t made) {
  const size_t length = Text_.size();
  auto &at = state.At;
  auto &decodedAttribute = state.DecodedAttribute;
  if (!NameStart(Text_[at])) {
    return Refuse("an attribute's name begins with a letter or an underscore", at);
  }
  const size_t attribute = at;
  while (at < length && NameChar(Text_[at])) { ++at; }
  const size_t attributeStop = at;
  while (at < length && Space(Text_[at])) { ++at; }
  if (at >= length || Text_[at] != '=') {
    return Refuse("an attribute carries a value, so its name is followed by '='", attribute);
  }
  ++at;
  while (at < length && Space(Text_[at])) { ++at; }
  if (at >= length || (Text_[at] != '"' && Text_[at] != '\'')) {
    return Refuse("an attribute's value is quoted", attribute);
  }
  const char quote = Text_[at];
  ++at;
  const size_t value = at;
  while (at < length && Text_[at] != quote) { ++at; }
  if (at >= length) { return Refuse("an attribute's value never closes", attribute); }
  if (Attributes_.size() >= kXmlMaxAttributes) {
    return Refuse("the document reaches the attribute bound of " +
                      std::to_string(kXmlMaxAttributes),
                  attribute);
  }
  Attribute one;
  one.NameOff = static_cast<uint32_t>(attribute);
  one.NameLen = static_cast<uint32_t>(attributeStop - attribute);
  one.ValueOff = static_cast<uint32_t>(value);
  if (!DecodeXmlAttribute(std::string_view(Text_).substr(value, at - value), decodedAttribute)) {
    return Refuse("invalid XML attribute value or character reference", value);
  }
  std::ranges::copy(decodedAttribute, Text_.begin() + static_cast<std::ptrdiff_t>(value));
  one.ValueLen = static_cast<uint32_t>(decodedAttribute.size());
  Attributes_.push_back(one);
  ++Nodes_[made].Attributes;
  ++at;
  return true;
}

}
