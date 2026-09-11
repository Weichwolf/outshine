#include <array>
#include <type_traits>
#include <expected>
#include <limits>
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

constexpr size_t kMaximumEntitySpan = 10;
constexpr size_t kMaximumMarkupBytes = size_t{1024} * 1024;
constexpr size_t kMaximumMarkupNodes = 65536;
constexpr size_t kMaximumMarkupAttributes = 65536;
constexpr size_t kMaximumMarkupDepth = 256;
static_assert(kMaximumMarkupNodes <= static_cast<size_t>(std::numeric_limits<int>::max()));

namespace Says {
constexpr auto SourceBudget = "UI markup exceeds its source byte budget";
constexpr auto NodeBudget = "UI markup exceeds its node budget";
constexpr auto AttributeBudget = "UI markup exceeds its attribute budget";
constexpr auto DepthBudget = "UI markup exceeds its nesting budget";
}

bool ResolveEntity(std::string_view name, std::string &out) {
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
    if (parsed <= 0 || std::cmp_greater(parsed, kUnicodeMost)) { return false; }
    AppendUtf8(out, static_cast<uint32_t>(parsed));
  } else {
    return false;
  }
  return true;
}

void Resolve(std::string_view raw, std::string &out) {
  for (size_t at = 0; at < raw.size();) {
    if (raw[at] != '&') {
      out.push_back(raw[at++]);
      continue;
    }
    const size_t extent = raw.substr(at, kMaximumEntitySpan + 1).find(';');
    if (extent == std::string_view::npos || !ResolveEntity(raw.substr(at + 1, extent - 1), out)) {
      out.push_back(raw[at++]);
      continue;
    }
    at += extent + 1;
  }
}

}

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
  return std::ranges::any_of(kVoid, [tag](const char *one) { return Matches(tag, one); });
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
  return std::ranges::any_of(kBlock, [tag](const char *one) { return Matches(tag, one); });
}

const std::string *Markup::AttributeOf(int node, std::string_view name) const {
  if (node < 0 || static_cast<size_t>(node) >= Nodes_.size()) { return nullptr; }
  for (const Attribute &one : Nodes_[static_cast<size_t>(node)].Attributes) {
    if (one.Name == name) { return &one.Value; }
  }
  return nullptr;
}

namespace {

struct MarkupParser {
  std::string_view Source;
  std::string &Error;
  std::vector<Node> Nodes;
  std::string Style;
  bool Scripted = false;
  std::vector<int> Open{0};
  std::string Text;
  size_t At = 0;
  size_t Attributes = 0;

  MarkupParser(std::string_view source, std::string &error) : Source(source), Error(error) {
    Nodes.push_back({.Kind = NodeKind::Element,
                     .Name = "#document",
                     .Text = "",
                     .Attributes = {},
                     .Children = {},
                     .Parent = -1});
  }

  int Push(Node node) {
    if (Nodes.size() >= kMaximumMarkupNodes) {
      Error = Says::NodeBudget;
      return -1;
    }
    node.Parent = Open.back();
    Nodes.push_back(std::move(node));
    const int index = static_cast<int>(Nodes.size() - 1);
    Nodes[static_cast<size_t>(Open.back())].Children.push_back(index);
    return index;
  }

  bool Flush() {
    if (Text.empty()) { return true; }
    if (Push({.Kind = NodeKind::Text,
              .Name = "",
              .Text = Text,
              .Attributes = {},
              .Children = {},
              .Parent = -1}) < 0) {
      return false;
    }
    Text.clear();
    return true;
  }

  void SkipSpace(size_t &cursor) const {
    while (cursor < Source.size() && Space(Source[cursor])) { ++cursor; }
  }

  Attribute ReadAttribute(size_t &cursor) const {
    const size_t from = cursor;
    while (cursor < Source.size() && !Space(Source[cursor]) && Source[cursor] != '=' &&
           Source[cursor] != '>' && Source[cursor] != '/') {
      ++cursor;
    }
    Attribute attribute;
    attribute.Name = Lower(Source.substr(from, cursor - from));
    SkipSpace(cursor);
    if (cursor >= Source.size() || Source[cursor] != '=') { return attribute; }
    ++cursor;
    SkipSpace(cursor);
    if (cursor < Source.size() && (Source[cursor] == '"' || Source[cursor] == '\'')) {
      const char quote = Source[cursor++];
      const size_t valueFrom = cursor;
      while (cursor < Source.size() && Source[cursor] != quote) { ++cursor; }
      Resolve(Source.substr(valueFrom, cursor - valueFrom), attribute.Value);
      if (cursor < Source.size()) { ++cursor; }
    } else {
      const size_t valueFrom = cursor;
      while (cursor < Source.size() && !Space(Source[cursor]) && Source[cursor] != '>') {
        ++cursor;
      }
      Resolve(Source.substr(valueFrom, cursor - valueFrom), attribute.Value);
    }
    return attribute;
  }

  std::expected<bool, std::string_view> ReadAttributes(Node &element, size_t &cursor) {
    bool selfClosing = false;
    while (cursor < Source.size()) {
      SkipSpace(cursor);
      if (cursor < Source.size() && Source[cursor] == '/') {
        selfClosing = true;
        ++cursor;
        continue;
      }
      if (cursor >= Source.size() || Source[cursor] == '>') { break; }
      auto attribute = ReadAttribute(cursor);
      if (attribute.Name.empty()) { continue; }
      if (Attributes >= kMaximumMarkupAttributes) { return std::unexpected(Says::AttributeBudget); }
      ++Attributes;
      element.Attributes.push_back(std::move(attribute));
    }
    return selfClosing;
  }

  bool ReadClosing(std::string_view name, size_t cursor) {
    const size_t end = Source.find('>', cursor);
    At = end == std::string_view::npos ? Source.size() : end + 1;
    if (ClosesItself(name)) {
      Error = "the document closes <" + std::string(name) +
              ">, which is a void element and has no end tag";
      return false;
    }
    if (!Flush()) { return false; }
    for (size_t depth = Open.size(); depth-- > 1;) {
      if (Nodes[static_cast<size_t>(Open[depth])].Name == name) {
        Open.resize(depth);
        break;
      }
    }
    return true;
  }

  bool ReadRawText(Node element) {
    const std::string closer = "</" + element.Name;
    const size_t end = Source.find(closer, At);
    if (end == std::string_view::npos) {
      Error = "the document opens <" + element.Name +
              "> and never closes it, so where its text ends is a guess rather than a reading";
      return false;
    }
    const std::string_view body = Source.substr(At, end - At);
    const bool script = element.Name == "script";
    const bool style = element.Name == "style";
    const bool title = element.Name == "title";
    const int node = Push(std::move(element));
    if (node < 0) { return false; }
    if (script) {
      Scripted = Scripted || body.find_first_not_of(" \t\r\n") != std::string_view::npos;
    }
    if (style) {
      Style.append(body);
      Style.push_back('\n');
    } else if (title) {
      if (Open.size() > kMaximumMarkupDepth) {
        Error = Says::DepthBudget;
        return false;
      }
      Open.push_back(node);
      std::string resolved;
      Resolve(body, resolved);
      if (Push({.Kind = NodeKind::Text,
                .Name = "",
                .Text = std::move(resolved),
                .Attributes = {},
                .Children = {},
                .Parent = -1}) < 0) {
        return false;
      }
      Open.pop_back();
    }
    const size_t past = Source.find('>', end);
    At = past == std::string_view::npos ? Source.size() : past + 1;
    return true;
  }

  bool ReadOpening(std::string name, size_t cursor) {
    Node element{.Kind = NodeKind::Element,
                 .Name = std::move(name),
                 .Text = "",
                 .Attributes = {},
                 .Children = {},
                 .Parent = -1};
    const auto selfClosing = ReadAttributes(element, cursor);
    if (!selfClosing) {
      Error = selfClosing.error();
      return false;
    }
    At = cursor < Source.size() && Source[cursor] == '>' ? cursor + 1 : Source.size();
    if (IsBlockLevel(element.Name) && Open.size() > 1 &&
        Nodes[static_cast<size_t>(Open.back())].Name == "p") {
      if (!Flush()) { return false; }
      Open.pop_back();
    }
    if (!Flush()) { return false; }
    if (HoldsRawText(element.Name)) { return ReadRawText(std::move(element)); }
    const bool opens = !*selfClosing && !ClosesItself(element.Name);
    const int node = Push(std::move(element));
    if (node < 0) { return false; }
    if (opens) {
      if (Open.size() > kMaximumMarkupDepth) {
        Error = Says::DepthBudget;
        return false;
      }
      Open.push_back(node);
    }
    return true;
  }

  bool ReadTag() {
    const bool closing = At + 1 < Source.size() && Source[At + 1] == '/';
    size_t cursor = At + (closing ? 2 : 1);
    const size_t nameFrom = cursor;
    while (cursor < Source.size() && !Space(Source[cursor]) && Source[cursor] != '>' &&
           Source[cursor] != '/') {
      ++cursor;
    }
    std::string name = Lower(Source.substr(nameFrom, cursor - nameFrom));
    if (name.empty()) {
      Text.push_back('<');
      ++At;
      return true;
    }
    return closing ? ReadClosing(name, cursor) : ReadOpening(std::move(name), cursor);
  }

  bool SkipIgnored() {
    if (Source.compare(At, 4, "<!--") == 0) {
      const size_t end = Source.find("-->", At + 4);
      At = end == std::string_view::npos ? Source.size() : end + 3;
      return true;
    }
    if (At + 1 < Source.size() && (Source[At + 1] == '!' || Source[At + 1] == '?')) {
      const size_t end = Source.find('>', At);
      At = end == std::string_view::npos ? Source.size() : end + 1;
      return true;
    }
    return false;
  }

  bool Run() {
    while (At < Source.size()) {
      if (Source[At] != '<') {
        const size_t next = Source.find('<', At);
        const size_t end = next == std::string_view::npos ? Source.size() : next;
        Resolve(Source.substr(At, end - At), Text);
        At = end;
        continue;
      }
      if (SkipIgnored()) { continue; }
      if (!ReadTag()) { return false; }
    }
    return Flush();
  }
};

}

bool Markup::Read(std::string_view markup, std::string &error) {
  static_assert(std::is_nothrow_move_assignable_v<Markup>);
  if (markup.size() > kMaximumMarkupBytes) {
    error = Says::SourceBudget;
    return false;
  }
  MarkupParser parser(markup, error);
  if (!parser.Run()) { return false; }
  Markup candidate;
  candidate.Nodes_ = std::move(parser.Nodes);
  candidate.Style_ = std::move(parser.Style);
  candidate.Scripted_ = parser.Scripted;
  candidate.Root_ = 0;
  *this = std::move(candidate);
  error.clear();
  return true;
}

}
