#include "Xml.h"

#include <array>
#include <algorithm>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace outshine {

namespace {

bool Space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool NameStart(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool NameChar(char c) {
  return NameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

std::string Where(size_t at) {
  return " at byte " + std::to_string(at);
}

} // namespace

bool Xml::Refuse(const std::string &why, size_t at) {
  Error_ = why + Where(at);
  Nodes_.clear();
  Attributes_.clear();
  Asked_.clear();
  Root_ = 0;
  return false;
}

std::string Xml::Ref::Name() const {
  if (!Valid()) { return {}; }
  const Node &node = From_->Nodes_[At_];
  return From_->Span(node.NameOff, node.NameLen);
}

std::string Xml::Ref::Text() const {
  if (!Valid()) { return {}; }
  const Node &node = From_->Nodes_[At_];
  return From_->Span(node.TextOff, node.TextLen);
}

uint32_t Xml::Ref::Asking(const char *attribute) const {
  if (!Valid() || attribute == nullptr) { return kNoAttribute; }
  const Node &node = From_->Nodes_[At_];
  const size_t want = std::strlen(attribute);
  for (uint32_t at = 0; at < node.Attributes; ++at) {
    const uint32_t which = node.FirstAttribute + at;
    const Attribute &one = From_->Attributes_[which];
    if (one.NameLen == want &&
        std::memcmp(From_->Text_.data() + one.NameOff, attribute, want) == 0) {
      From_->Asked_[which] = 1;
      return which;
    }
  }
  return kNoAttribute;
}

bool Xml::Ref::Has(const char *attribute) const {
  return Asking(attribute) != kNoAttribute;
}

bool Xml::Ref::Spelt(const char *attribute) const {
  if (!Valid() || attribute == nullptr) { return false; }
  const Node &node = From_->Nodes_[At_];
  const size_t want = std::strlen(attribute);
  for (uint32_t at = 0; at < node.Attributes; ++at) {
    const Attribute &one = From_->Attributes_[node.FirstAttribute + at];
    if (one.NameLen == want && one.ValueLen != 0 &&
        std::memcmp(From_->Text_.data() + one.NameOff, attribute, want) == 0) {
      return true;
    }
  }
  return false;
}

std::string Xml::Ref::Attr(const char *attribute, const char *whenAbsent) const {
  const uint32_t which = Asking(attribute);
  if (which == kNoAttribute) { return {whenAbsent}; }
  const Attribute &one = From_->Attributes_[which];
  return From_->Span(one.ValueOff, one.ValueLen);
}

double Xml::Ref::Num(const char *attribute, double whenAbsent) const {
  if (!Has(attribute)) { return whenAbsent; }
  const std::string value = Attr(attribute);
  char *end = nullptr;
  const double read = std::strtod(value.c_str(), &end);
  if (end == value.c_str()) { return whenAbsent; }
  return read;
}

long long Xml::Ref::Int(const char *attribute, long long whenAbsent) const {
  if (!Has(attribute)) { return whenAbsent; }
  const std::string value = Attr(attribute);
  char *end = nullptr;
  const long long read = std::strtoll(value.c_str(), &end, 10);
  if (end == value.c_str()) { return whenAbsent; }
  return read;
}

bool Xml::Ref::Flag(const char *attribute, bool whenAbsent) const {
  if (!Has(attribute)) { return whenAbsent; }
  const std::string value = Attr(attribute);
  if (value == "true" || value == "1") { return true; }
  if (value == "false" || value == "0") { return false; }
  return whenAbsent;
}

size_t Xml::Ref::AttributeCount() const {
  if (!Valid()) { return 0; }
  return From_->Nodes_[At_].Attributes;
}

std::string Xml::Ref::AttributeAt(size_t which) const {
  if (!Valid()) { return {}; }
  const Node &node = From_->Nodes_[At_];
  if (which >= node.Attributes) { return {}; }
  const Attribute &one = From_->Attributes_[node.FirstAttribute + which];
  return From_->Span(one.NameOff, one.NameLen);
}

Xml::Ref Xml::Ref::First() const {
  if (!Valid()) { return {}; }
  return {From_, From_->Nodes_[At_].FirstChild};
}

Xml::Ref Xml::Ref::Next() const {
  if (!Valid()) { return {}; }
  ++From_->SiblingSteps_;
  return {From_, From_->Nodes_[At_].NextSibling};
}

Xml::Ref Xml::Ref::Child(const char *name) const {
  return At(name, 0);
}

bool Xml::Ref::Siblings::Iterator::Named() const {
  if (Name_ == nullptr) { return true; }
  const Node &node = From_->Nodes_[At_];
  return node.NameLen == Want_ &&
         std::memcmp(From_->Text_.data() + node.NameOff, Name_, Want_) == 0;
}

void Xml::Ref::Siblings::Iterator::Settle() {
  while (At_ != 0 && !Named()) {
    ++From_->SiblingSteps_;
    At_ = From_->Nodes_[At_].NextSibling;
  }
}

Xml::Ref::Siblings::Iterator &Xml::Ref::Siblings::Iterator::operator++() {
  if (At_ == 0) { return *this; }
  ++From_->SiblingSteps_;
  At_ = From_->Nodes_[At_].NextSibling;
  Settle();
  return *this;
}

Xml::Ref::Siblings Xml::Ref::Children(const char *name) const {
  if (!Valid()) { return {}; }
  return {From_, From_->Nodes_[At_].FirstChild, name};
}

size_t Xml::Ref::Count(const char *name) const {
  if (!Valid() || name == nullptr) { return 0; }
  const size_t want = std::strlen(name);
  size_t found = 0;
  for (uint32_t at = From_->Nodes_[At_].FirstChild; at != 0; at = From_->Nodes_[at].NextSibling) {
    ++From_->SiblingSteps_;
    const Node &node = From_->Nodes_[at];
    if (node.NameLen == want && std::memcmp(From_->Text_.data() + node.NameOff, name, want) == 0) {
      ++found;
    }
  }
  return found;
}

Xml::Ref Xml::Ref::At(const char *name, size_t which) const {
  if (!Valid() || name == nullptr) { return {}; }
  const size_t want = std::strlen(name);
  size_t seen = 0;
  for (uint32_t at = From_->Nodes_[At_].FirstChild; at != 0; at = From_->Nodes_[at].NextSibling) {
    ++From_->SiblingSteps_;
    const Node &node = From_->Nodes_[at];
    if (node.NameLen != want || std::memcmp(From_->Text_.data() + node.NameOff, name, want) != 0) {
      continue;
    }
    if (seen == which) { return {From_, at}; }
    ++seen;
  }
  return {};
}

bool Xml::Parse(const char *text, size_t length) {
  Error_.clear();
  SiblingSteps_ = 0;
  Nodes_.clear();
  Attributes_.clear();
  Asked_.clear();
  Root_ = 0;
  if (text == nullptr) { return Refuse("there is no document to read", 0); }

  Text_.assign(text, length);
  Nodes_.emplace_back();

  std::array<uint32_t, kXmlMaxDepth> stack{};
  size_t depth = 0;
  size_t at = 0;
  bool closed = false;

  while (at < length) {
    if (Text_[at] != '<') {
      const size_t from = at;
      while (at < length && Text_[at] != '<') { ++at; }
      if (depth > 0) {
        size_t start = from;
        size_t stop = at;
        while (start < stop && Space(Text_[start])) { ++start; }
        while (stop > start && Space(Text_[stop - 1])) { --stop; }
        if (stop > start) {
          Node &into = Nodes_[stack[depth - 1]];
          into.TextOff = static_cast<uint32_t>(start);
          into.TextLen = static_cast<uint32_t>(stop - start);
        }
      }
      continue;
    }

    if (at + 1 < length && Text_[at + 1] == '?') {
      const size_t stop = Text_.find("?>", at);
      if (stop == std::string::npos) { return Refuse("a processing instruction never ends", at); }
      at = stop + 2;
      continue;
    }
    if (at + 3 < length && Text_.compare(at, 4, "<!--") == 0) {
      const size_t stop = Text_.find("-->", at);
      if (stop == std::string::npos) { return Refuse("a comment never ends", at); }
      at = stop + 3;
      continue;
    }
    if (at + 1 < length && Text_[at + 1] == '!') {
      return Refuse("this reader takes elements, attributes and text, and a declaration beginning "
                    "'<!' is a doctype or a section it does not",
                    at);
    }

    if (at + 1 < length && Text_[at + 1] == '/') {
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
      while (stop < length && Text_[stop] != '>') { ++stop; }
      if (stop >= length) { return Refuse("a closing tag never ends", at); }
      --depth;
      if (depth == 0) { closed = true; }
      at = stop + 1;
      continue;
    }

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
      return Refuse("the document reaches the element bound of " + std::to_string(kXmlMaxNodes),
                    at);
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
      if (Nodes_[parent].FirstChild == 0) {
        Nodes_[parent].FirstChild = made;
      } else {
        uint32_t last = Nodes_[parent].FirstChild;
        while (Nodes_[last].NextSibling != 0) { last = Nodes_[last].NextSibling; }
        Nodes_[last].NextSibling = made;
      }
    }

    at = stop;
    bool empty = false;
    while (at < length) {
      while (at < length && Space(Text_[at])) { ++at; }
      if (at < length && Text_[at] == '/') {
        empty = true;
        ++at;
        continue;
      }
      if (at < length && Text_[at] == '>') {
        ++at;
        break;
      }
      if (at >= length) { return Refuse("an element's tag never ends", name); }
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
      one.ValueLen = static_cast<uint32_t>(at - value);
      Attributes_.push_back(one);
      ++Nodes_[made].Attributes;
      ++at;
    }

    if (!empty) {
      if (depth >= kXmlMaxDepth) {
        return Refuse("the document nests past the depth bound of " + std::to_string(kXmlMaxDepth),
                      name);
      }
      stack[depth++] = made;
    } else if (depth == 0) {
      closed = true;
    }
  }

  if (depth != 0) {
    const Node &open = Nodes_[stack[depth - 1]];
    return Refuse("the element '" + Span(open.NameOff, open.NameLen) + "' is never closed", length);
  }
  if (Root_ == 0) { return Refuse("the document carries no element", length); }
  Asked_.assign(Attributes_.size(), 0);
  return true;
}

Xml::Unread Xml::FirstUnread() const {
  const auto unasked = std::ranges::find(Asked_, 0);
  if (unasked == Asked_.end()) { return Unread{}; }
  const uint32_t wanted = static_cast<uint32_t>(unasked - Asked_.begin());

  struct Standing {
    uint32_t At = 0;
    uint32_t Next = 0;
    size_t PathWas = 0;
  };

  std::vector<Standing> walk;
  walk.reserve(kXmlDeepestChain);
  std::string path;

  if (Root_ == 0) { return Unread{}; }
  const Node &root = Nodes_[Root_];
  path.append(Text_.data() + root.NameOff, root.NameLen);
  walk.push_back(Standing{.At = Root_, .Next = root.FirstChild, .PathWas = 0});

  while (!walk.empty()) {
    Standing &here = walk.back();
    const Node &node = Nodes_[here.At];
    if (wanted >= node.FirstAttribute && wanted < node.FirstAttribute + node.Attributes) {
      const Attribute &one = Attributes_[wanted];
      return Unread{.Path = path, .Attribute = Span(one.NameOff, one.NameLen)};
    }
    if (here.Next == 0) {
      path.resize(here.PathWas);
      walk.pop_back();
      continue;
    }
    const uint32_t child = here.Next;
    here.Next = Nodes_[child].NextSibling;
    const Node &under = Nodes_[child];
    const size_t was = path.size();
    path.push_back('/');
    path.append(Text_.data() + under.NameOff, under.NameLen);
    walk.push_back(Standing{.At = child, .Next = under.FirstChild, .PathWas = was});
  }
  return Unread{};
}

} // namespace outshine
