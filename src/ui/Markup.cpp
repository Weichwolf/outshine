#include <array>
#include <charconv>
#include "Utf8.h"
#include "Markup.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <string>
#include <utility>
#include <vector>
#include <utility>

namespace outshine::Ui {

constexpr uint32_t kUnicodeMost = 0x10FFFF;

constexpr char kNbspTail = static_cast<char>(0xA0);

constexpr char kNbspLead = static_cast<char>(0xC2);

namespace {

bool Matches(std::string_view a, const char *b) {
  return a == std::string_view(b);
}

char Lowered(char c) {
  return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string Lower(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) { out.push_back(Lowered(c)); }
  return out;
}

bool Space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

void Resolve(std::string_view raw, std::string &out) {
  for (size_t at = 0; at < raw.size();) {
    if (raw[at] != '&') {
      out.push_back(raw[at++]);
      continue;
    }
    const size_t end = raw.find(';', at);
    if (end == std::string_view::npos || end - at > 10) {
      out.push_back(raw[at++]);
      continue;
    }
    const std::string_view name = raw.substr(at + 1, end - at - 1);
    if (Matches(name, "amp")) {
      out.push_back('&');
    } else if (Matches(name, "lt")) {
      out.push_back('<');
    } else if (Matches(name, "gt")) {
      out.push_back('>');
    } else if (Matches(name, "quot")) {
      out.push_back('"');
    } else if (Matches(name, "apos")) {
      out.push_back('\'');
    } else if (Matches(name, "nbsp")) {
      out.push_back(kNbspLead);
      out.push_back(kNbspTail);
    } else if (!name.empty() && name[0] == '#') {
      const bool hex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
      const std::string_view digits = name.substr(hex ? 2 : 1);
      long parsed = 0;
      (void)std::from_chars(digits.data(), digits.data() + digits.size(), parsed, hex ? 16 : 10);
      if (parsed <= 0 || std::cmp_greater(parsed, kUnicodeMost)) {
        out.push_back('&');
        ++at;
        continue;
      }
      const auto code = static_cast<uint32_t>(parsed);

      AppendUtf8(out, code);
    } else {
      out.push_back('&');
      ++at;
      continue;
    }
    at = end + 1;
  }
}

} // namespace

bool ClosesItself(std::string_view tag) {
  static const std::array<const char *const, 14> kVoid = {{"area",
                                                           "base",
                                                           "br",
                                                           "col",
                                                           "embed",
                                                           "hr",
                                                           "img",
                                                           "input",
                                                           "link",
                                                           "meta",
                                                           "param",
                                                           "source",
                                                           "track",
                                                           "wbr"}};
  for (const char *one : kVoid) {
    if (Matches(tag, one)) { return true; }
  }
  return false;
}

bool HoldsRawText(std::string_view tag) {
  return Matches(tag, "style") || Matches(tag, "script") || Matches(tag, "title");
}

bool IsBlockLevel(std::string_view tag) {
  static const std::array<const char *const, 26> kBlock = {
      {"address", "article", "aside",   "blockquote", "div",  "dl",  "fieldset",
       "figure",  "footer",  "form",    "h1",         "h2",   "h3",  "h4",
       "h5",      "h6",      "header",  "hr",         "main", "nav", "ol",
       "p",       "pre",     "section", "table",      "ul"}};
  for (const char *one : kBlock) {
    if (Matches(tag, one)) { return true; }
  }
  return false;
}

const std::string *Markup::AttributeOf(int node, std::string_view name) const {
  if (node < 0 || static_cast<size_t>(node) >= Nodes_.size()) { return nullptr; }
  for (const Attribute &one : Nodes_[static_cast<size_t>(node)].Attributes) {
    if (one.Name == name) { return &one.Value; }
  }
  return nullptr;
}

bool Markup::Read(std::string_view markup, std::string &error) {
  Nodes_.clear();
  Style_.clear();
  Root_ = -1;

  Nodes_.push_back({.Kind = NodeKind::Element,
                    .Name = "#document",
                    .Text = "",
                    .Attributes = {},
                    .Children = {},
                    .Parent = -1});
  Root_ = 0;
  std::vector<int> open{0};

  const auto push = [&](Node node) {
    node.Parent = open.back();
    Nodes_.push_back(std::move(node));
    const int at = static_cast<int>(Nodes_.size()) - 1;
    Nodes_[static_cast<size_t>(open.back())].Children.push_back(at);
    return at;
  };

  size_t at = 0;
  std::string text;
  const auto flush = [&] {
    if (text.empty()) { return; }
    push({.Kind = NodeKind::Text,
          .Name = "",
          .Text = text,
          .Attributes = {},
          .Children = {},
          .Parent = -1});
    text.clear();
  };

  while (at < markup.size()) {
    if (markup[at] != '<') {
      const size_t next = markup.find('<', at);
      const size_t end = next == std::string_view::npos ? markup.size() : next;
      Resolve(markup.substr(at, end - at), text);
      at = end;
      continue;
    }

    if (markup.compare(at, 4, "<!--") == 0) {
      const size_t end = markup.find("-->", at + 4);
      at = end == std::string_view::npos ? markup.size() : end + 3;
      continue;
    }
    if (at + 1 < markup.size() && (markup[at + 1] == '!' || markup[at + 1] == '?')) {
      const size_t end = markup.find('>', at);
      at = end == std::string_view::npos ? markup.size() : end + 1;
      continue;
    }
    const bool closing = at + 1 < markup.size() && markup[at + 1] == '/';
    size_t cursor = at + (closing ? 2 : 1);
    const size_t nameFrom = cursor;
    while (cursor < markup.size() && !Space(markup[cursor]) && markup[cursor] != '>' &&
           markup[cursor] != '/') {
      ++cursor;
    }
    const std::string name = Lower(markup.substr(nameFrom, cursor - nameFrom));
    if (name.empty()) {
      text.push_back('<');
      at += 1;
      continue;
    }

    if (closing) {
      const size_t end = markup.find('>', cursor);
      at = end == std::string_view::npos ? markup.size() : end + 1;
      if (ClosesItself(name)) {
        error = "the document closes <" + name + ">, which is a void element and has no end tag";
        return false;
      }
      flush();

      size_t found = open.size();
      for (size_t depth = open.size(); depth-- > 1;) {
        if (Nodes_[static_cast<size_t>(open[depth])].Name == name) {
          found = depth;
          break;
        }
      }
      if (found < open.size()) { open.resize(found); }
      continue;
    }

    Node element{.Kind = NodeKind::Element,
                 .Name = name,
                 .Text = "",
                 .Attributes = {},
                 .Children = {},
                 .Parent = -1};
    bool selfClosing = false;
    while (cursor < markup.size()) {
      while (cursor < markup.size() && Space(markup[cursor])) { ++cursor; }
      if (cursor < markup.size() && markup[cursor] == '/') {
        selfClosing = true;
        ++cursor;
        continue;
      }
      if (cursor >= markup.size() || markup[cursor] == '>') { break; }
      const size_t from = cursor;
      while (cursor < markup.size() && !Space(markup[cursor]) && markup[cursor] != '=' &&
             markup[cursor] != '>' && markup[cursor] != '/') {
        ++cursor;
      }
      Attribute attribute;
      attribute.Name = Lower(markup.substr(from, cursor - from));
      while (cursor < markup.size() && Space(markup[cursor])) { ++cursor; }
      if (cursor < markup.size() && markup[cursor] == '=') {
        ++cursor;
        while (cursor < markup.size() && Space(markup[cursor])) { ++cursor; }
        if (cursor < markup.size() && (markup[cursor] == '"' || markup[cursor] == '\'')) {
          const char quote = markup[cursor++];
          const size_t valueFrom = cursor;
          while (cursor < markup.size() && markup[cursor] != quote) { ++cursor; }
          Resolve(markup.substr(valueFrom, cursor - valueFrom), attribute.Value);
          if (cursor < markup.size()) { ++cursor; }
        } else {
          const size_t valueFrom = cursor;
          while (cursor < markup.size() && !Space(markup[cursor]) && markup[cursor] != '>') {
            ++cursor;
          }
          Resolve(markup.substr(valueFrom, cursor - valueFrom), attribute.Value);
        }
      }
      if (!attribute.Name.empty()) { element.Attributes.push_back(std::move(attribute)); }
    }
    at = cursor < markup.size() && markup[cursor] == '>' ? cursor + 1 : markup.size();

    if (IsBlockLevel(name) && open.size() > 1 &&
        Nodes_[static_cast<size_t>(open.back())].Name == "p") {
      flush();
      open.pop_back();
    }
    flush();

    if (HoldsRawText(name)) {
      const std::string closer = "</" + name;
      const size_t end = markup.find(closer, at);
      if (end == std::string_view::npos) {
        error = "the document opens <" + name +
                "> and never closes it, so where its text ends is a "
                "guess rather than a reading";
        return false;
      }
      const std::string_view body = markup.substr(at, end - at);
      const int node = push(std::move(element));
      if (name == "script") {
        Scripted_ = Scripted_ || body.find_first_not_of(" \t\r\n") != std::string_view::npos;
      }
      if (name == "style") {
        Style_.append(body);
        Style_.push_back('\n');
      } else if (name == "title") {
        const int held = open.back();
        open.push_back(node);
        std::string resolved;
        Resolve(body, resolved);
        push({.Kind = NodeKind::Text,
              .Name = "",
              .Text = resolved,
              .Attributes = {},
              .Children = {},
              .Parent = -1});
        open.back() = held;
        open.pop_back();
      }
      const size_t past = markup.find('>', end);
      at = past == std::string_view::npos ? markup.size() : past + 1;
      continue;
    }

    const int node = push(std::move(element));
    if (!selfClosing && !ClosesItself(name)) { open.push_back(node); }
  }
  flush();
  return true;
}

} // namespace outshine::Ui
