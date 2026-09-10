#include "Xml.h"

#include <algorithm>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace outshine {

namespace {

std::string Where(size_t at) {
  return " at byte " + std::to_string(at);
}

}

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

std::optional<std::string> Xml::Ref::Said(const char *attribute) const {
  const uint32_t which = Asking(attribute);
  if (which == kNoAttribute) { return std::nullopt; }
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

}
