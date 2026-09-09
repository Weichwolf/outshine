#include "Utf8.h"
#include "Json.h"

#include <charconv>
#include <limits>

#include "DecimalEdge.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <vector>
#include <system_error>
#include <string_view>

namespace outshine {

constexpr size_t kEscapeDigits = 4;
constexpr size_t kPairEscapeLength = 6;
constexpr int kHexBase = 16;

bool Json::Parse(const char *text, size_t len) {
  Ok_ = false;
  Nodes_.clear();
  Kids_.clear();
  P_ = 0;
  Depth_ = 0;
  if (text == nullptr || len > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    Text_.clear();
    return false;
  }
  Text_.assign(text, len);
  Ok_ = ParseValue() == 0;
  if (Ok_) {
    Skip();
    Ok_ = P_ == Text_.size();
  }
  return Ok_;
}

void Json::Skip() {
  while (P_ < Text_.size()) {
    const char c = Text_[P_];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      P_++;
    } else {
      break;
    }
  }
}

std::optional<Json::Quoted> Json::ParseString() {
  if (P_ >= Text_.size() || Text_[P_] != '"') { return std::nullopt; }
  P_++;
  const size_t start = P_;
  bool escaped = false;
  while (P_ < Text_.size()) {
    const char c = Text_[P_];
    if (c == '\\') {
      escaped = true;
      ++P_;
      if (!ParseEscape()) { return std::nullopt; }
      continue;
    }
    if (static_cast<unsigned char>(c) < 0x20u) { return std::nullopt; }
    if (c == '"') {
      const Quoted said{.Off = static_cast<uint32_t>(start),
                        .Len = static_cast<uint32_t>(P_ - start),
                        .Escaped = escaped};
      P_++;
      return said;
    }
    P_++;
  }
  return std::nullopt;
}

bool Json::ParseEscape() {
  if (P_ >= Text_.size()) { return false; }
  const char escaped = Text_[P_++];
  if (escaped != 'u') { return std::string_view("\"\\/bfnrt").contains(escaped); }
  if (Text_.size() - P_ < kEscapeDigits) { return false; }
  unsigned code = 0;
  const char *begin = Text_.data() + P_;
  const auto parsed = std::from_chars(begin, begin + kEscapeDigits, code, kHexBase);
  if (parsed.ec != std::errc{} || parsed.ptr != begin + kEscapeDigits) { return false; }
  P_ += kEscapeDigits;
  return true;
}

int32_t Json::ParseValue() {
  Skip();
  if (P_ >= Text_.size()) { return -1; }
  if (Depth_ >= kMostDepth) { return -1; }
  ++Depth_;
  const size_t from = P_;
  const int32_t id = ParseValueInside();
  --Depth_;
  if (id >= 0) {
    Nodes_[static_cast<size_t>(id)].From = static_cast<uint32_t>(from);
    Nodes_[static_cast<size_t>(id)].To = static_cast<uint32_t>(P_);
  }
  return id;
}

namespace {
size_t SkipDigits(std::string_view text, size_t at) {
  while (at < text.size() && text[at] >= '0' && text[at] <= '9') { ++at; }
  return at;
}

std::optional<size_t> NumberEnd(std::string_view text, size_t at) {
  if (at < text.size() && text[at] == '-') { ++at; }
  const size_t whole = at;
  at = at < text.size() && text[at] == '0' ? at + 1 : SkipDigits(text, at);
  if (at == whole) { return std::nullopt; }
  if (at < text.size() && text[at] == '.') {
    const size_t fraction = ++at;
    at = SkipDigits(text, at);
    if (at == fraction) { return std::nullopt; }
  }
  if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
    ++at;
    if (at < text.size() && (text[at] == '+' || text[at] == '-')) { ++at; }
    const size_t exponent = at;
    at = SkipDigits(text, at);
    if (at == exponent) { return std::nullopt; }
  }
  return at;
}
}

int32_t Json::ParseNumber(int32_t id) {
  const auto end = NumberEnd(Text_, P_);
  if (!end) { return -1; }
  const size_t at = *end;
  double v = 0.0;
  const auto scanned = std::from_chars(Text_.c_str() + P_, Text_.c_str() + at, v);
  if (scanned.ec == std::errc::result_out_of_range) {
    const std::string_view span(Text_.c_str() + P_, at - P_);
    const double magnitude = DecimalEdge(span) == Edge::Zero ? 0.0 : 1.7976931348623157e308;
    v = Text_[P_] == '-' ? -magnitude : magnitude;
  } else if (scanned.ec != std::errc() || scanned.ptr != Text_.c_str() + at) {
    return -1;
  }
  P_ = at;
  Nodes_[static_cast<size_t>(id)].K = Kind::Number;
  Nodes_[static_cast<size_t>(id)].Num = v;
  return id;
}

bool Json::ParseLiteral(std::string_view word) {
  if (!std::string_view(Text_).substr(P_).starts_with(word)) { return false; }
  P_ += word.size();
  return true;
}

int32_t Json::ParseMember() {
  const auto key = ParseString();
  if (!key) { return -1; }
  Skip();
  if (P_ >= Text_.size() || Text_[P_] != ':') { return -1; }
  ++P_;
  const int32_t kid = ParseValue();
  if (kid < 0) { return -1; }
  Node &node = Nodes_[static_cast<size_t>(kid)];
  node.Key = key->Off;
  node.KeyLen = key->Len;
  node.KeyEscaped = key->Escaped;
  return kid;
}

int32_t Json::ParseContainer(int32_t id, bool object) {
  const char close = object ? '}' : ']';
  ++P_;
  std::vector<int32_t> kids;
  Skip();
  if (P_ < Text_.size() && Text_[P_] != close) {
    for (;;) {
      const int32_t kid = object ? ParseMember() : ParseValue();
      if (kid < 0) { return -1; }
      kids.push_back(kid);
      Skip();
      if (P_ >= Text_.size() || Text_[P_] != ',') { break; }
      ++P_;
      Skip();
    }
  }
  if (P_ >= Text_.size() || Text_[P_] != close) { return -1; }
  ++P_;
  Node &node = Nodes_[static_cast<size_t>(id)];
  node.K = object ? Kind::Object : Kind::Array;
  node.First = static_cast<uint32_t>(Kids_.size());
  node.Count = static_cast<uint32_t>(kids.size());
  Kids_.insert(Kids_.end(), kids.begin(), kids.end());
  return id;
}

int32_t Json::ParseValueInside() {
  const auto id = static_cast<int32_t>(Nodes_.size());
  Nodes_.emplace_back();
  const char c = Text_[P_];
  if (c == '{' || c == '[') { return ParseContainer(id, c == '{'); }
  if (c == '"') {
    const auto said = ParseString();
    if (!said) { return -1; }
    Node &node = Nodes_[static_cast<size_t>(id)];
    node.K = Kind::String;
    node.Str = said->Off;
    node.StrLen = said->Len;
    node.Escaped = said->Escaped;
    return id;
  }
  if (ParseLiteral("true")) {
    Nodes_[static_cast<size_t>(id)].K = Kind::Bool;
    Nodes_[static_cast<size_t>(id)].Num = 1.0;
    return id;
  }
  if (ParseLiteral("false")) {
    Nodes_[static_cast<size_t>(id)].K = Kind::Bool;
    return id;
  }
  if (ParseLiteral("null")) {
    Nodes_[static_cast<size_t>(id)].K = Kind::Null;
    return id;
  }
  return ParseNumber(id);
}

std::string Json::Decode(uint32_t off, uint32_t len, bool escaped) const {
  if (!escaped) { return Text_.substr(off, len); }
  std::string out;
  out.reserve(len);
  for (uint32_t i = 0; i < len; i++) {
    const char c = Text_[off + i];
    if (c != '\\' || i + 1 >= len) {
      out.push_back(c);
      continue;
    }
    const char e = Text_[off + ++i];
    switch (e) {
      case 'n': out.push_back('\n'); break;
      case 't': out.push_back('\t'); break;
      case 'r': out.push_back('\r'); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;

      case 'u': {
        if (i + kEscapeDigits >= len) { break; }
        unsigned cp = static_cast<unsigned>(
            std::strtoul(Text_.substr(off + i + 1, kEscapeDigits).c_str(), nullptr, kHexBase));
        i += kEscapeDigits;
        if (IsHighSurrogate(cp) && i + kPairEscapeLength < len && Text_[off + i + 1] == '\\' &&
            Text_[off + i + 2] == 'u') {
          const unsigned low = static_cast<unsigned>(
              std::strtoul(Text_.substr(off + i + 3, kEscapeDigits).c_str(), nullptr, kHexBase));
          if (IsLowSurrogate(low)) {
            cp = PairedSurrogates(cp, low);
            i += kPairEscapeLength;
          }
        }
        if (IsSurrogate(cp)) { cp = kReplacement; }
        AppendUtf8(out, cp);
        break;
      }
      default: out.push_back(e); break;
    }
  }
  return out;
}

Json::Ref Json::Ref::operator[](size_t i) const {
  if (!Valid()) { return {}; }
  const Json::Node &n = Doc->Nodes_[static_cast<size_t>(Node)];
  if (i >= n.Count) { return {}; }
  return {Doc, Doc->Kids_[n.First + i]};
}

std::string_view Json::Ref::Source() const {
  if (!Valid()) { return {}; }
  const Json::Node &n = Doc->Nodes_[static_cast<size_t>(Node)];
  if (n.To <= n.From || n.To > Doc->Text_.size()) { return {}; }
  return std::string_view(Doc->Text_).substr(n.From, n.To - n.From);
}

std::string Json::Ref::Key(size_t i) const {
  if (!Valid()) { return {}; }
  const Json::Node &n = Doc->Nodes_[static_cast<size_t>(Node)];
  if (n.K != Kind::Object || i >= n.Count) { return {}; }
  const Json::Node &c = Doc->Nodes_[static_cast<size_t>(Doc->Kids_[n.First + i])];
  return Doc->Decode(c.Key, c.KeyLen, c.KeyEscaped);
}

Json::Ref Json::Ref::operator[](const char *key) const {
  if (!Valid() || (key == nullptr)) { return {}; }
  const Json::Node &n = Doc->Nodes_[static_cast<size_t>(Node)];
  if (n.K != Kind::Object) { return {}; }
  const size_t klen = std::strlen(key);
  for (uint32_t i = 0; i < n.Count; i++) {
    const int32_t kid = Doc->Kids_[n.First + i];
    const Json::Node &c = Doc->Nodes_[static_cast<size_t>(kid)];
    if (c.KeyEscaped) {
      if (Doc->Decode(c.Key, c.KeyLen, true) == key) { return {Doc, kid}; }
    } else if (c.KeyLen == klen && (std::memcmp(Doc->Text_.c_str() + c.Key, key, klen) == 0)) {
      return {Doc, kid};
    }
  }
  return {};
}

double Json::Ref::Num(double def) const {
  if (!Valid()) { return def; }
  const Json::Node &n = Doc->Nodes_[static_cast<size_t>(Node)];
  return n.K == Kind::Number ? n.Num : def;
}

bool Json::Ref::Bool(bool def) const {
  if (!Valid()) { return def; }
  const Json::Node &n = Doc->Nodes_[static_cast<size_t>(Node)];
  return n.K == Kind::Bool ? n.Num != 0.0 : def;
}

std::string Json::Ref::Str(const char *def) const {
  if (!Valid()) { return def; }
  const Json::Node &n = Doc->Nodes_[static_cast<size_t>(Node)];
  return n.K == Kind::String ? Doc->Decode(n.Str, n.StrLen, n.Escaped) : std::string(def);
}

bool Json::Ref::StrEquals(const char *s) const {
  if (!Valid() || (s == nullptr)) { return false; }
  const Json::Node &n = Doc->Nodes_[static_cast<size_t>(Node)];
  if (n.K != Kind::String) { return false; }
  if (n.Escaped) { return Doc->Decode(n.Str, n.StrLen, true) == s; }
  const size_t l = std::strlen(s);
  return n.StrLen == l && (std::memcmp(Doc->Text_.c_str() + n.Str, s, l) == 0);
}

}
